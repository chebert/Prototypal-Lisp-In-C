#include "read2.h"

#include <assert.h>
#include <string.h>

#include "byte_vector.h"
#include "log.h"
#include "memory.h"
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

// For EvaluateFunction's
#define GOTO(dest)  BEGIN  next = (dest); return;  END
#define ERROR(error_code) BEGIN  error = error_code; GOTO(ReadError);  END
#define CONTINUE GOTO(GetContinue())

#define CHECK(op)  BEGIN  (op); if (error) { GOTO(ReadError); }  END
#define SAVE(reg)  BEGIN  CHECK(Save((reg), &error));  END

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

void ReadObject2(u64 *return_index, enum ErrorCode *return_error) {
  next_index = *return_index;
  next = ReadDispatch;

  while (next) next();

  *return_error = error;
  *return_index = next_index;
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

    LOG(LOG_READ, "reading first element of list/pair");
    SAVE(REGISTER_READ_STACK);
    SetRegister(REGISTER_READ_STACK, nil);

    SAVE(REGISTER_CONTINUE);
    SetContinue(ReadListContinue);

    GOTO(ReadDispatch);
  }
}

void ReadListContinue() {
  LOG(LOG_READ, "finished read element of list/pair");
  LOG_OP(LOG_READ, PrintlnObject(GetExpression()));
  CHECK(PushExpressionOntoReadStack(&error));

  DiscardWhitespaceAndComments();
  u8 ch = ReadCharacter();
  if (ch == ')') {
    // End of list
    LOG(LOG_READ, "read end of list");
    SetExpression(ReverseInPlace(GetRegister(REGISTER_READ_STACK), nil));
    Restore(REGISTER_CONTINUE);
    Restore(REGISTER_READ_STACK);
    CONTINUE;
  } else if (ch == '.') {
    // Pair separator or part of a number/symbol?
    u8 next_ch = ReadCharacter();
    if (IsWhitespace(next_ch)) {
      LOG(LOG_READ, "read pair separator");
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
      LOG(LOG_READ, "reading another object");
      GOTO(ReadDispatch);
    }
  } else {
    // Another object
    UnreadCharacter();
    LOG(LOG_READ, "reading another object");
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
  // Discard the final " from the string
  u64 length = next_index - start_index - 1;

  Object bytes;

  // Copy the contents into a new string.
  CHECK(bytes = AllocateByteVector(length + 1, &error));
  u8 *source = StringCharacterBuffer(GetRegister(REGISTER_READ_SOURCE));
  for (u64 i = 0; i < length; ++i)
    UnsafeByteVectorSet(bytes, i, source[i + start_index]);
  UnsafeByteVectorSet(bytes, length, 0);

  SetExpression(BoxString(bytes));
  CONTINUE;
}

u8 *ReadSource() {
  return StringCharacterBuffer(GetRegister(REGISTER_READ_SOURCE));
}

u8 ReadCharacter() {
  u8 *source = ReadSource();
  LOG(LOG_READ, "Reading character %c", source[next_index]);
  return source[next_index++];
}
void UnreadCharacter() {
  --next_index;
  LOG(LOG_READ, "Unreading character %c", ReadSource()[next_index]);
}

#define MAXIMUM_SYMBOL_LENGTH 512
const u8 *CopySourceString(const u8 *source, u64 length, enum ErrorCode *error) {
  static u8 source_buffer[MAXIMUM_SYMBOL_LENGTH + 1];
  if (length >= MAXIMUM_SYMBOL_LENGTH) {
    LOG_ERROR("Symbol length too long: %llu", length);
    *error = ERROR_READ_SYMBOL_OR_NUMBER_TOO_LONG;
    return 0;
  }
  memcpy(source_buffer, source, length);
  source_buffer[length] = 0;
  LOG(LOG_READ, "Copied \"%s\" to source buffer \"%s\"", source, source_buffer);
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
  CHECK(data = CopySourceString(&ReadSource()[start_index], length, &error));

  if (IsInteger(data, length)) {
    s64 value;
    if (!sscanf(data, "%lld", &value))
      ERROR(ERROR_READ_COULD_NOT_READ_INTEGER);

    SetExpression(BoxFixnum(value));
    LOG(LOG_READ, "Read fixnum");
    LOG_OP(LOG_READ, PrintlnObject(GetExpression()));
  } else if (IsReal(data, length, &exponent_marker)) {
    real64 value;
    if (!sscanf(data, "%lf", &value))
      ERROR(ERROR_READ_COULD_NOT_READ_REAL);

    SetExpression(BoxReal64(value));
    LOG(LOG_READ, "Read real64");
    LOG_OP(LOG_READ, PrintlnObject(GetExpression()));
  } else if (!strcmp(data, ".")) {
    ERROR(ERROR_READ_INVALID_PAIR_SEPARATOR);
  } else {
    CHECK(SetExpression(InternSymbol(data, &error)));
    LOG(LOG_READ, "Read symbol: %s from %s", data);
    LOG_OP(LOG_READ, PrintlnObject(GetExpression()));
  }
  CONTINUE;
}

void ReadError() {
  LOG_ERROR("Error: %s", ErrorCodeString(error));
  LOG_ERROR("next_index: %llu", next_index);
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
  return IsWhitespace(ch) || ch == ')' || ch == '(' || ch == '\'' || ch == ';' || ch == '"' || ch == '\0';
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

static b64 SymbolEq(Object symbol, const u8 *name) {
  return IsSymbol(symbol) && !strcmp(StringCharacterBuffer(symbol), name);
}

void TestRead2() {
  enum ErrorCode error = NO_ERROR;
  InitializeMemory(512, &error);
  InitializeSymbolTable(1, &error);
  InternSymbol("quote", &error);

  // Read string
  {
    const u8* source = "\"abra\"";
    SetRegister(REGISTER_READ_SOURCE, AllocateString(source, &error));
    u64 index = 0;
    ReadObject2(&index, &error);
    assert(!error);
    assert(IsString(GetExpression()));
    assert(!strcmp("abra", StringCharacterBuffer(GetExpression())));
  }

  {
    const u8 *source = "-12345";
    SetRegister(REGISTER_READ_SOURCE, AllocateString(source, &error));
    u64 index = 0;
    ReadObject2(&index, &error);
    assert(!error);
    assert(IsFixnum(GetExpression()));
    assert(-12345 == UnboxFixnum(GetExpression()));
    assert(index == 6);
  }

  // Read real64
  {
    const u8* source = "-123.4e5";
    SetRegister(REGISTER_READ_SOURCE, AllocateString(source, &error));
    u64 index = 0;
    ReadObject2(&index, &error);
    assert(!error);
    assert(IsReal64(GetRegister(REGISTER_EXPRESSION)));
    assert(-123.4e5 == UnboxReal64(GetRegister(REGISTER_EXPRESSION)));
  }

  // Read symbol
  {
    const u8* source = "the-symbol";
    SetRegister(REGISTER_READ_SOURCE, AllocateString(source, &error));
    u64 index = 0;
    ReadObject2(&index, &error);
    assert(!error);
    assert(SymbolEq(GetRegister(REGISTER_EXPRESSION), "the-symbol"));
    assert(!strcmp("the-symbol", StringCharacterBuffer(GetRegister(REGISTER_EXPRESSION))));
  }

  // Read ()
  {
    const u8* source = " (     \n)";
    SetRegister(REGISTER_READ_SOURCE, AllocateString(source, &error));
    u64 index = 0;
    ReadObject2(&index, &error);
    assert(!error);
    assert(IsNil(GetRegister(REGISTER_EXPRESSION)));
  }

  // Read Pair
  {
    const u8* source = " (a . b)";
    SetRegister(REGISTER_READ_SOURCE, AllocateString(source, &error));
    u64 index = 0;
    ReadObject2(&index, &error);
    assert(!error);
    assert(IsPair(GetRegister(REGISTER_EXPRESSION)));
    assert(SymbolEq(Car(GetRegister(REGISTER_EXPRESSION)), "a"));
    assert(SymbolEq(Cdr(GetRegister(REGISTER_EXPRESSION)), "b"));
  }

  // Read List
  {
    const u8* source = " (a)";
    SetRegister(REGISTER_READ_SOURCE, AllocateString(source, &error));
    u64 index = 0;
    ReadObject2(&index, &error);
    assert(!error);
    assert(IsPair(GetRegister(REGISTER_EXPRESSION)));
    assert(SymbolEq(Car(GetRegister(REGISTER_EXPRESSION)), "a"));
    assert(IsNil(Cdr(GetRegister(REGISTER_EXPRESSION))));
  }

  // Read Dotted list: nested
  {
    const u8* source = " ((a . b) (c . d) . (e . f))";
    //                    st      uv         w
    SetRegister(REGISTER_READ_SOURCE, AllocateString(source, &error));
    u64 index = 0;
    ReadObject2(&index, &error);
    assert(!error);
    Object s = GetRegister(REGISTER_EXPRESSION);
    Object t = Car(s);
    Object u = Cdr(s);
    Object v = Car(u);
    Object w = Cdr(u);

    assert(SymbolEq(Car(t), "a"));
    assert(SymbolEq(Cdr(t), "b"));
    assert(SymbolEq(Car(v), "c"));
    assert(SymbolEq(Cdr(v), "d"));
    assert(SymbolEq(Car(w), "e"));
    assert(SymbolEq(Cdr(w), "f"));
  }

  // Read list
  {
    const u8* source = "(a b (c d . e) (f g) h)";
    //                   s t uv w      xy z  S
    SetRegister(REGISTER_READ_SOURCE, AllocateString(source, &error));
    u64 index = 0;
    ReadObject2(&index, &error);
    assert(!error);
    Object s = GetRegister(REGISTER_EXPRESSION);
    Object t = Cdr(s);
    Object u = Cdr(t);
    Object v = Car(u);
    Object w = Cdr(v);
    Object x = Cdr(u);
    Object y = Car(x);
    Object z = Cdr(y);
    Object S = Cdr(x);

    assert(SymbolEq(Car(s), "a"));
    assert(SymbolEq(Car(t), "b"));
    assert(SymbolEq(Car(v), "c"));
    assert(SymbolEq(Car(w), "d"));
    assert(SymbolEq(Cdr(w), "e"));
    assert(SymbolEq(Car(y), "f"));
    assert(SymbolEq(Car(z), "g"));
    assert(IsNil(Cdr(z)));
    assert(SymbolEq(Car(S), "h"));
    assert(IsNil(Cdr(S)));
  }

  // Read quoted
  {
    const u8* source = "'(a b)";
    // (quote . ((a . (b . nil)) . nil))
    //  s        tu    v
    SetRegister(REGISTER_READ_SOURCE, AllocateString(source, &error));
    u64 index = 0;
    ReadObject2(&index, &error);
    assert(!error);
    Object s = GetRegister(REGISTER_EXPRESSION);
    Object t = Cdr(s);
    Object u = Car(t);
    Object v = Cdr(u);

    assert(SymbolEq(Car(s), "quote"));
    assert(SymbolEq(Car(u), "a"));
    assert(SymbolEq(Car(v), "b"));
  }

  DestroyMemory();
}
