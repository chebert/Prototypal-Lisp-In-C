#include "read2.h"

#include <string.h>

#include "byte_vector.h"
#include "log.h"
#include "pair.h"
#include "root.h"
#include "string.h"
#include "symbol_table.h"
#include "tag.h"

#define BEGIN do { 
#define END   } while(0)

static EvaluateFunction next;
static enum ErrorCode error;
static u64 next_index;

// For EvaluateFunction's
#define GOTO(dest)  BEGIN  next = (dest); return;  END
#define ERROR(error_code) BEGIN  error = error_code; GOTO(ReadError);  END
#define CONTINUE GOTO(GetContinue())

#define CHECK(op)  BEGIN  (op); if (error) { GOTO(ReadError); }  END
#define SAVE(reg)  BEGIN  CHECK(Save((reg), &error));  END

static b64 IsWhitespace(u8 ch);
static b64 IsTerminating(u8 ch);
static b64 IsNumberSign(u8 ch);
static b64 IsDigit(u8 ch);
static b64 IsExponentMarker(u8 ch);

static void ConsumeChar(const u8 **source, u64 *length);
static void OptionalNumberSign(const u8 **source, u64 *length);
static u64 CountDigits(const u8 **source, u64 *length);

u8 *ReadSource();
u8 ReadCharacter();
void UnreadCharacter();
const u8 *CopySourceString(const u8 *source, u64 length, enum ErrorCode *error);
void PushExpressionOntoReadStack(enum ErrorCode *error);
Object ReverseInPlace(Object list, Object last_cdr);

void DiscardComment();
void DiscardWhitespaceAndComments();

void ReadDispatch();

void ReadList();
void ReadListContinue();
void ReadEndOfDottedList();

void ReadQuotedObject();
void ReadQuotedObjectFinished();

void ReadString();
void ReadNumberOrSymbol();

void ReadError();

// True if source is pointing at: decimal-point digit* [exponent]
// if at_least_one_decimal_digit is true, then source must be pointing at:
//   decimal-point digit+ [exponent] | decimal-point exponent
static b64 IsDecimalAndOptionalExponent(const u8 **source, u64 *length, u8 *exponent_marker, b64 at_least_one_decimal_digit);
// True if source is pointing at an exponent.
// Exponent-marker is filled with the Real32 or Real64 exponent marker if true.
static b64 IsExponent(const u8 **source, u64 *length, u8 *exponent_marker);

// True if source is pointing at an integer.
static b64 IsInteger(const u8 *source, u64 length);
// True if source is pointing at a Real.
// exponent_marker is filled with the exponent marker of the real (defaulting to 'e').
static b64 IsReal(const u8 *source, u64 length, u8 *exponent_marker);

Object ReadObject2(u64 *return_index, enum ErrorCode *return_error) {
  next_index = *return_index;
  next = ReadDispatch;

  while (next) {
    next();
    if (error) {
      *return_error = error;
      return nil;
    }
  }

  *return_index = next_index;
  return GetExpression();
}

// Leaves the start_index at the first non-comment character
void DiscardComment() {
  u8 ch;
  for (ch = ReadCharacter(); ch == '\0' || ch != '\n'; ch = ReadCharacter())
    ;
  if (ch == '\n') ReadCharacter();
}

// Leaves the next character at the first non-comment & non-whitespace character
void DiscardWhitespaceAndComments() {
  for (u8 ch = ReadCharacter(); IsWhitespace(ch) || ch == ';'; ch = ReadCharacter()) {
    if (ch == ';')
      DiscardComment();
  }
  UnreadCharacter();
}

void ReadDispatch() {
  DiscardWhitespaceAndComments();
  u8 ch = ReadCharacter();
  if (ch == '(')       { GOTO(ReadList); }
  else if (ch == '\'') { GOTO(ReadQuotedObject); }
  else if (ch == '"')  { GOTO(ReadString); }
  else if (ch == '\0') { ERROR(ERROR_READ_UNEXPECTED_EOF); }
  else if (ch == ')')  { ERROR(ERROR_READ_UNMATCHED_LIST_CLOSE); }
  else {
    UnreadCharacter();
    GOTO(ReadNumberOrSymbol);
  }
}

void ReadList() {
  DiscardWhitespaceAndComments();
  u8 ch = ReadCharacter();
  if (ch == ')') {
    SetExpression(nil);
    CONTINUE;
  } else {
    UnreadCharacter();

    SAVE(REGISTER_READ_STACK);
    SetRegister(REGISTER_READ_STACK, nil);

    SAVE(REGISTER_CONTINUE);
    SetContinue(ReadListContinue);

    GOTO(ReadDispatch);
  }
}

void ReadListContinue() {
  CHECK(PushExpressionOntoReadStack(&error));

  DiscardWhitespaceAndComments();
  u8 ch = ReadCharacter();
  if (ch == ')') {
    // End of list
    SetExpression(ReverseInPlace(GetRegister(REGISTER_READ_STACK), nil));
    Restore(REGISTER_CONTINUE);
    Restore(REGISTER_READ_STACK);
    CONTINUE;
  } else if (ch == '.') {
    // Pair separator or part of a number/symbol?
    u8 next_ch = ReadCharacter();
    if (IsWhitespace(next_ch)) {
      // Pair separator
      SetContinue(ReadEndOfDottedList);
      GOTO(ReadDispatch);
    } else if (next_ch == '\0') {
      // EOF
      ERROR(ERROR_READ_UNTERMINATED_PAIR);
    } else {
      // Part of a number/symbol
      UnreadCharacter();
      UnreadCharacter();
      GOTO(ReadDispatch);
    }
  } else {
    // Another object
    UnreadCharacter();
    GOTO(ReadDispatch);
  }
}

void ReadEndOfDottedList() {
  SetExpression(ReverseInPlace(GetRegister(REGISTER_READ_STACK), GetExpression()));

  DiscardWhitespaceAndComments();
  u8 ch = ReadCharacter();
  if (ch != ')')  {
    ERROR(ERROR_READ_DOTTED_LIST_EXPECTED_LIST_CLOSE);
  }

  Restore(REGISTER_CONTINUE);
  Restore(REGISTER_READ_STACK);
  CONTINUE;
}

void ReadQuotedObject() {
  SAVE(REGISTER_CONTINUE);
  SetContinue(ReadQuotedObjectFinished);
  GOTO(ReadDispatch);
}

void ReadQuotedObjectFinished() {
  Object pair;

  // (quoted-object)
  CHECK(pair = AllocatePair(&error));
  SetCar(pair, GetExpression());
  SetCdr(pair, nil);
  SetExpression(pair);

  // (quote quoted-object)
  CHECK(pair = AllocatePair(&error));
  SetCar(pair, FindSymbol("quote"));
  SetCdr(pair, GetExpression());
  SetExpression(pair);

  Restore(REGISTER_CONTINUE);
  CONTINUE;
}

void ReadString() {
  u64 start_index = next_index;
  for (u8 ch = ReadCharacter(); ch != '"'; ch = ReadCharacter()) {
    if (ch == '\\') ReadCharacter();
    if (ch == '\0') ERROR(ERROR_READ_UNTERMINATED_STRING);
  }
  u64 length = next_index - start_index;

  Object bytes;

  // Copy the contents into a new string.
  CHECK(bytes = AllocateByteVector(1 + length, &error));
  u8 *source = StringCharacterBuffer(GetRegister(REGISTER_READ_SOURCE));
  for (u64 i = 0; i < length; ++i)
    UnsafeByteVectorSet(bytes, i, source[i + start_index]);
  UnsafeByteVectorSet(bytes, 1+length, 0);

  SetExpression(BoxString(bytes));
  CONTINUE;
}

u8 *ReadSource() {
  return StringCharacterBuffer(GetRegister(REGISTER_READ_SOURCE));
}

u8 ReadCharacter() {
  u8 *source = ReadSource();
  return source[next_index++];
}
void UnreadCharacter() {
  --next_index;
}

#define MAXIMUM_SYMBOL_LENGTH 512
const u8 *CopySourceString(const u8 *source, u64 length, enum ErrorCode *error) {
  static u8 source_buffer[MAXIMUM_SYMBOL_LENGTH + 1];
  if (length >= MAXIMUM_SYMBOL_LENGTH) {
    *error = ERROR_READ_SYMBOL_OR_NUMBER_TOO_LONG;
  }
  memcpy(source_buffer, source, length + 1);
  source_buffer[length] = 0;
  return source_buffer;
}

// Integer := [sign] {digit} {digit}*
// real := [sign]           decimal-point {digit}+ [exponent]
//       | [sign] {digit}+  decimal-point {digit}* [exponent]
//       | [sign] {digit}+ [decimal-point {digit}*] exponent
// exponent := (e | E) [sign] {digit+}
// otherwise symbol
void ReadNumberOrSymbol() {
  u64 start_index = next_index;
  for (u8 ch = ReadCharacter(); !IsTerminating(ch); ch = ReadCharacter())
    ;
  UnreadCharacter();

  u64 length = next_index - start_index;

  u8 exponent_marker;
  const u8 *data;
  CHECK(data = CopySourceString(ReadSource(), length, &error));

  if (IsInteger(data, length)) {
    s64 value;
    if (!sscanf(data, "%lld", &value))
      ERROR(ERROR_READ_COULD_NOT_READ_INTEGER);

    SetExpression(BoxFixnum(value));
  } else if (IsReal(data, length, &exponent_marker)) {
    real64 value;
    if (!sscanf(data, "%lf", &value))
      ERROR(ERROR_READ_COULD_NOT_READ_REAL);

    SetExpression(BoxReal64(value));
  } else if (!strcmp(data, ".")) {
    ERROR(ERROR_READ_INVALID_PAIR_SEPARATOR);
  } else {
    CHECK(SetExpression(InternSymbol(data, &error)));
  }
  CONTINUE;
}

void ReadError() {
  LOG_ERROR("Error reading.");
  next = 0;
}

void PushExpressionOntoReadStack(enum ErrorCode *error) {
  Object new_stack = AllocatePair(error);
  if (*error) return;

  SetCdr(new_stack, GetRegister(REGISTER_READ_STACK));
  SetRegister(REGISTER_READ_STACK, new_stack);
  SetCar(new_stack, GetExpression());
}

Object ReverseInPlace(Object list, Object last_cdr) {
  Object result = list;
  list = Rest(list);

  SetCdr(result, last_cdr);

  while (!IsNil(list)) {
    // Pop the next cons off the list
    Object next = list;
    list = Rest(list);

    // Push it onto the front of the result
    SetCdr(next, result);
    result = next;
  }
  return result;
}

b64 IsWhitespace(u8 ch) {
  return ch == ' ' 
      || ch == '\t'
      || ch == '\n'
      || ch == '\r'
      || ch == '\v'
      || ch == '\f';
}

b64 IsTerminating(u8 ch) {
  return IsWhitespace(ch) || ch == ')' || ch == '(' || ch == '\'' || ch == ';' || ch == '"';
}

b64 IsNumberSign(u8 ch) {
  return ch == '+' || ch == '-';
}

b64 IsDigit(u8 ch) {
  return '0' <= ch && ch <= '9';
}

b64 IsExponentMarker(u8 ch) {
  return ch == 'e' || ch == 'E';
}

void ConsumeChar(const u8 **source, u64 *length) {
  *source = *source + 1;
  *length = *length - 1;
}

void OptionalNumberSign(const u8 **source, u64 *length) {
  if (IsNumberSign(**source)) {
    ConsumeChar(source, length);
  }
}

u64 CountDigits(const u8 **source, u64 *length) {
  u64 num_digits = 0;
  for (; IsDigit(**source) && *length > 0; ConsumeChar(source, length)) {
    ++num_digits;
  }
  return num_digits;
}

// Integer := [sign] {digit}+
b64 IsInteger(const u8 *source, u64 length) {
  OptionalNumberSign(&source, &length);
  u64 num_digits = CountDigits(&source, &length);
  return num_digits >= 1 && length == 0;
}

b64 IsExponent(const u8 **source, u64 *length, u8 *exponent_marker) {
  // exponent := exponent-marker [sign] {digit+}
  if (*length == 0) return 0;

  if (!IsExponentMarker(**source)) return 0;
  *exponent_marker = **source;
  ConsumeChar(source, length);

  // exponent := [sign] {digit+}
  OptionalNumberSign(source, length);

  // exponent := {digit+}
  return CountDigits(source, length) > 0;
}

b64 IsDecimalAndOptionalExponent(const u8 **source, u64 *length, u8 *exponent_marker, b64 at_least_one_decimal_digit) {
  if (*length == 0) return 0;
  // real := decimal-point {digit}+ [exponent]
  //       | decimal-point exponent

  if (**source != '.') return 0;
  ConsumeChar(source, length);

  // real := {digit}+
  //       | {digit}* exponent
  u64 num_digits = CountDigits(source, length);
  return (*length == 0 && (!at_least_one_decimal_digit || num_digits > 0)) || IsExponent(source, length, exponent_marker);
}

b64 IsReal(const u8 *source, u64 length, u8 *exponent_marker) {
  *exponent_marker = 'e';
  // real := [sign] {digit}*    decimal-point {digit}*   [exponent]
  //       | [sign] {digit}+  [ decimal-point {digit}* ]  exponent
  OptionalNumberSign(&source, &length);
  u64 num_digits = CountDigits(&source, &length);
  if (num_digits == 0) {
    // real := decimal-point {digit}* [exponent]
    return IsDecimalAndOptionalExponent(&source, &length, exponent_marker, 1);
  } else {
    // real :=   decimal-point {digit}*   [exponent]
    //       | [ decimal-point {digit}* ]  exponent
    if (length == 0) return 0;

    if (*source == '.') {
      // real := decimal-point {digit}* [exponent]
      return IsDecimalAndOptionalExponent(&source, &length, exponent_marker, 0);
    } else if (IsExponent(&source, &length, exponent_marker)) {
      // real := exponent
      return length == 0;
    } else {
      return 0;
    }
  }
}
