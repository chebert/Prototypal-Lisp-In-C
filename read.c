#include "read.h"

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

static Object GetReadResult();
static void SetReadResult(Object value);

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

// Dot := (.)
//   "."
// Sign? := (+|-)?
//   "+", "-", ""
// Digit := (0|1|2|3|4|5|6|7|8|9)
//   "3" "2"
// exponent-marker := (e|E)
//   "e" "E"
// signed-digits+ := {sign?}{digit}+
//   "+1" "-28" "1234"
// signed-digits* := {sign?}{digit}*
//   "-" "+" "-123" ""
// Exponent := {exponent-marker}{signed-digits+}
//   "e+234" "E-1" "e87"
// Leading-decimal := {sign?}{dot}{digit}+
//   "+.123" "-.4" ".875"
// Non-leading-decimal := {signed-digits+}{dot}{digit}*
//   "-3." "+42.8" "7.6"
// Decimal := ({non-leading-decimal}|{leading-decimal})
//    "+.123" "-3."
// terminating-character := ({whitespace}|')'|'('|'|;|'"'|EOF)
// Pair separator := {dot}
//   "."
// Real := (({decimal}[exponent])|({signed-digits*}{exponent}))
//   "+42.8" ".875e+234"
//   -e-4 +123E+234 678e48
// Integer := {signed-digits+}
//   "-1" "+234" "8" 
// Symbol := (.*)

static b64 IsDot(u8 ch);
static b64 IsNumberSign(u8 ch);
static b64 IsDigit(u8 ch);
static b64 IsExponentMarker(u8 ch);
static b64 IsWhitespace(u8 ch);
static b64 IsTerminating(u8 ch);

struct ParseState {
  const u8 *source;
  u64 end_index;
  u64 index;
};
static b64 TestNext(const struct ParseState *parse_state, b64 (*predicate)(u8 ch));
static b64 ConsumeOneOptional(struct ParseState *parse_state, b64 (*predicate)(u8 ch));
static b64 ConsumeZeroOrMore(struct ParseState *parse_state, b64 (*predicate)(u8 ch));
static b64 ConsumeOne(struct ParseState *parse_state, b64 (*predicate)(u8 ch));
static b64 ConsumeOneOrMore(struct ParseState *parse_state, b64 (*predicate)(u8 ch));
static b64 IsFullyParsed(struct ParseState *parse_state);

static b64 ConsumeOptional(struct ParseState *parse_state, b64 (*consume)(struct ParseState *parse_state));

static b64 ConsumeZeroOrMoreSignedDigits(struct ParseState *parse_state);
static b64 ConsumeOneOrMoreSignedDigits(struct ParseState *parse_state);
static b64 ConsumeExponent(struct ParseState *parse_state);
static b64 ConsumeLeadingDecimal(struct ParseState *parse_state);
static b64 ConsumeNonLeadingDecimal(struct ParseState *parse_state);
static b64 ConsumeDecimal(struct ParseState *parse_state);

static b64 ConsumeDecimalAndOptionalExponent(struct ParseState *parse_state);
static b64 ConsumeZeroOrMoreSignedDigitsAndExponent(struct ParseState *parse_state);
static b64 ConsumeReal(struct ParseState *parse_state);

// Rename: IsInteger/IsReal
static b64 IsInteger(struct ParseState parse_state);
static b64 IsReal(struct ParseState parse_state);

static b64 TestNext(const struct ParseState *parse_state, b64 (*predicate)(u8 ch)) {
  u64 index = parse_state->index;
  return index < parse_state->end_index && predicate(parse_state->source[index]);
}

#define SUCCEED return 1
#define FAIL return 0

// Sequences:
// Returns 0 if the test fails.
#define DO(test) BEGIN if (!(test)) FAIL; END
#define THEN(test) DO(test)
// Returns the value of test.
#define FINALLY(test) return (test);

// Alternatives:
// Saves the parse state, and tries op. If it succeeds, returns true.
// Otherwise restores the saved parse state.
#define TRY(parse_state, op) \
  BEGIN \
    struct ParseState saved = *parse_state; \
    if (op) return 1; \
    *parse_state = saved; \
  END

b64 ConsumeOne(struct ParseState *parse_state, b64 (*predicate)(u8 ch)) {
  if (TestNext(parse_state, predicate)) {
    ++parse_state->index;
    SUCCEED;
  }
  FAIL;
}
b64 ConsumeOneOptional(struct ParseState *parse_state, b64 (*predicate)(u8 ch)) {
  if (TestNext(parse_state, predicate))
    ++parse_state->index;
  SUCCEED;
}
b64 ConsumeZeroOrMore(struct ParseState *parse_state, b64 (*predicate)(u8 ch)) {
  for (; TestNext(parse_state, predicate); ++parse_state->index)
    ;
  SUCCEED;
}
b64 ConsumeOneOrMore(struct ParseState *parse_state, b64 (*predicate)(u8 ch)) {
  if (TestNext(parse_state, predicate)) {
    ++parse_state->index;
    return ConsumeZeroOrMore(parse_state, predicate);
  }
  FAIL;
}
b64 IsFullyParsed(struct ParseState *parse_state) {
  return parse_state->end_index == parse_state->index;
}

b64 ConsumeOptional(struct ParseState *parse_state, b64 (*consume)(struct ParseState *)) {
  TRY(parse_state, consume(parse_state));
  FAIL;
}

// signed-digits+ := {sign?}{digit}+
b64 ConsumeOneOrMoreSignedDigits(struct ParseState *parse_state) {
  // {sign?}{digit}+
  DO(ConsumeOneOptional(parse_state, IsNumberSign));
  // {digit}+
  THEN(ConsumeOneOrMore(parse_state, IsDigit));
  SUCCEED;
}

// signed-digits* := {sign?}{digit}*
b64 ConsumeZeroOrMoreSignedDigits(struct ParseState *parse_state) {
  // {sign?}{digit}*
  DO(ConsumeOneOptional(parse_state, IsNumberSign));
  // {digit}*
  THEN(ConsumeZeroOrMore(parse_state, IsDigit));
  SUCCEED;
}

// Exponent := {exponent-marker}{signed-digits+}
b64 ConsumeExponent(struct ParseState *parse_state) {
  DO(ConsumeOne(parse_state, IsExponentMarker));
  THEN(ConsumeOneOrMoreSignedDigits(parse_state));
  SUCCEED;
}

// Leading-decimal := {sign?}{dot}{digit}+
b64 ConsumeLeadingDecimal(struct ParseState *parse_state) {
  // {sign?}{dot}{digit}+
  DO(ConsumeOneOptional(parse_state, IsNumberSign));
  // {dot}{digit}+
  THEN(ConsumeOne(parse_state, IsDot));
  // {digit}+
  THEN(ConsumeOneOrMore(parse_state, IsDigit));
  SUCCEED;
}

// Non-leading-decimal := {signed-digits+}{dot}{digit}*
b64 ConsumeNonLeadingDecimal(struct ParseState *parse_state) {
  // {signed-digits+}{dot}{digit}*
  DO(ConsumeOneOrMoreSignedDigits(parse_state));
  // {dot}{digit}*
  THEN(ConsumeOne(parse_state, IsDot));
  // {digit}*
  THEN(ConsumeZeroOrMore(parse_state, IsDigit));
  SUCCEED;
}

// Decimal := ({non-leading-decimal}|{leading-decimal})
b64 ConsumeDecimal(struct ParseState *parse_state) {
  // {leading-decimal} OR {non-leading-decimal}
  TRY(parse_state, ConsumeLeadingDecimal(parse_state));
  // {non-leading-decimal}
  TRY(parse_state, ConsumeNonLeadingDecimal(parse_state));
  FAIL;
}

b64 ConsumeDecimalAndOptionalExponent(struct ParseState *parse_state) {
  DO(ConsumeDecimal(parse_state));
  THEN(ConsumeOptional(parse_state, ConsumeExponent));
  SUCCEED;
}

b64 ConsumeZeroOrMoreSignedDigitsAndExponent(struct ParseState *parse_state) {
  DO(ConsumeZeroOrMoreSignedDigits(parse_state));
  THEN(ConsumeExponent(parse_state));
  SUCCEED;
}

b64 ConsumeReal(struct ParseState *parse_state) {
  TRY(parse_state, ConsumeDecimalAndOptionalExponent(parse_state));
  TRY(parse_state, ConsumeZeroOrMoreSignedDigitsAndExponent(parse_state));
  FAIL;
}

b64 IsInteger(struct ParseState parse_state) {
  DO(ConsumeOneOrMoreSignedDigits(&parse_state));
  return IsFullyParsed(&parse_state);
}

// Real := (({decimal}[exponent])|({signed-digits*}{exponent}))
b64 IsReal(struct ParseState parse_state) {
  DO(ConsumeReal(&parse_state));
  return IsFullyParsed(&parse_state);
}
#undef DO
#undef TRY
#undef FAIL
#undef SUCCEED
#undef FINALLY

Object ReadFromString(Object string, s64 *position, enum ErrorCode *return_error) {
  SetRegister(REGISTER_READ_SOURCE, string);

  next_index = *position;

  Save(REGISTER_CONTINUE, return_error);
  if (*return_error) return nil;

  SetContinue(0);
  next = ReadDispatch;
  while (next) next();
  Restore(REGISTER_CONTINUE);

  *return_error = error;
  *position = next_index;

  return GetReadResult();
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
  for (u8 ch = ReadCharacter(); 1; ch = ReadCharacter()) {
    if (ch == ';')
      DiscardComment();
    else if (!IsWhitespace(ch))
      break;
  }
  // Just read a non-comment non-whitespace character. Unread it.
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
    SetReadResult(nil);
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
  LOG_OP(LOG_READ, PrintlnObject(GetReadResult()));
  CHECK(PushExpressionOntoReadStack(&error));

  DiscardWhitespaceAndComments();
  u8 ch = ReadCharacter();
  if (ch == ')') {
    // End of list
    LOG(LOG_READ, "read end of list");
    SetReadResult(ReverseInPlace(GetRegister(REGISTER_READ_STACK), nil));
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
  SetReadResult(ReverseInPlace(GetRegister(REGISTER_READ_STACK), GetReadResult()));

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
  SetCar(pair, GetReadResult());
  SetCdr(pair, nil);
  SetReadResult(pair);

  // (quote quoted-object)
  CHECK(pair = AllocatePair(&error));
  SetCar(pair, FindSymbol("quote"));
  SetCdr(pair, GetReadResult());
  SetReadResult(pair);

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

  SetReadResult(BoxString(bytes));
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

void ReadNumberOrSymbol() {
  u64 start_index = next_index;
  for (u8 ch = ReadCharacter(); !IsTerminating(ch); ch = ReadCharacter())
    ;
  UnreadCharacter();

  u64 length = next_index - start_index;

  u8 exponent_marker;
  const u8 *data;
  CHECK(data = CopySourceString(&ReadSource()[start_index], length, &error));

  struct ParseState parse_state;
  parse_state.source = data;
  parse_state.end_index = length;
  parse_state.index = 0;

  if (IsInteger(parse_state)) {
    s64 value;
    if (!sscanf(data, "%lld", &value))
      ERROR(ERROR_READ_COULD_NOT_READ_INTEGER);

    SetReadResult(BoxFixnum(value));
    LOG(LOG_READ, "Read fixnum");
    LOG_OP(LOG_READ, PrintlnObject(GetReadResult()));
  } else if (IsReal(parse_state)) {
    real64 value;
    if (!sscanf(data, "%lf", &value))
      ERROR(ERROR_READ_COULD_NOT_READ_REAL);

    SetReadResult(BoxReal64(value));
    LOG(LOG_READ, "Read real64");
    LOG_OP(LOG_READ, PrintlnObject(GetReadResult()));
  } else {
    CHECK(SetReadResult(InternSymbol(data, &error)));
    LOG(LOG_READ, "Read symbol: %s from %s", data);
    LOG_OP(LOG_READ, PrintlnObject(GetReadResult()));
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
  SetCar(new_stack, GetReadResult());
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

b64 IsDot(u8 ch) { return ch == '.'; }
b64 IsNumberSign(u8 ch) { return ch == '+' || ch == '-'; }
b64 IsDigit(u8 ch) { return '0' <= ch && ch <= '9'; }
b64 IsExponentMarker(u8 ch) { return ch == 'e' || ch == 'E'; }

static b64 SymbolEq(Object symbol, const u8 *name) {
  return IsSymbol(symbol) && !strcmp(StringCharacterBuffer(symbol), name);
}

Object GetReadResult() { return GetRegister(REGISTER_READ_RESULT); }
void SetReadResult(Object value) { SetRegister(REGISTER_READ_RESULT, value); }

void TestRead() {
  enum ErrorCode error = NO_ERROR;
  InitializeMemory(512, &error);
  InitializeSymbolTable(1, &error);
  InternSymbol("quote", &error);

  // Read string
  {
    const u8* source = "\"abra\"";
    s64 position = 0;
    Object result = ReadFromString(AllocateString(source, &error), &position, &error);
    assert(!error);
    assert(IsString(result));
    assert(!strcmp("abra", StringCharacterBuffer(result)));
  }

  {
    const u8 *source = "-12345";
    s64 position = 0;
    Object result = ReadFromString(AllocateString(source, &error), &position, &error);
    assert(!error);
    assert(IsFixnum(result));
    assert(-12345 == UnboxFixnum(result));
    assert(position == 6);
  }

  // Read real64
  {
    const u8* source = "-123.4e5";
    s64 position = 0;
    Object result = ReadFromString(AllocateString(source, &error), &position, &error);
    assert(!error);
    assert(IsReal64(result));
    assert(-123.4e5 == UnboxReal64(result));
  }

  // Read symbol
  {
    const u8* source = "the-symbol";
    s64 position = 0;
    Object result = ReadFromString(AllocateString(source, &error), &position, &error);
    assert(!error);
    assert(SymbolEq(result, "the-symbol"));
    assert(!strcmp("the-symbol", StringCharacterBuffer(result)));
  }

  // Read ()
  {
    const u8* source = " (     \n)";
    s64 position = 0;
    Object result = ReadFromString(AllocateString(source, &error), &position, &error);
    assert(!error);
    assert(IsNil(result));
  }

  // Read Pair
  {
    const u8* source = " (a . b)";
    s64 position = 0;
    Object result = ReadFromString(AllocateString(source, &error), &position, &error);
    assert(!error);
    assert(IsPair(result));
    assert(SymbolEq(Car(result), "a"));
    assert(SymbolEq(Cdr(result), "b"));
  }

  // Read List
  {
    const u8* source = " (a)";
    s64 position = 0;
    Object result = ReadFromString(AllocateString(source, &error), &position, &error);
    assert(!error);
    assert(IsPair(result));
    assert(SymbolEq(Car(result), "a"));
    assert(IsNil(Cdr(result)));
  }

  // Read Dotted list: nested
  {
    const u8* source = " ((a . b) (c . d) . (e . f))";
    //                    st      uv         w
    s64 position = 0;
    Object result = ReadFromString(AllocateString(source, &error), &position, &error);
    assert(!error);
    Object s = result;
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
    s64 position = 0;
    Object result = ReadFromString(AllocateString(source, &error), &position, &error);
    assert(!error);
    Object s = result;
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
    s64 position = 0;
    Object result = ReadFromString(AllocateString(source, &error), &position, &error);
    assert(!error);
    Object s = result;
    Object t = Cdr(s);
    Object u = Car(t);
    Object v = Cdr(u);

    assert(SymbolEq(Car(s), "quote"));
    assert(SymbolEq(Car(u), "a"));
    assert(SymbolEq(Car(v), "b"));
  }

  DestroyMemory();
}
