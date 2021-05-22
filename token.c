#include "token.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c_types.h"

// Reads the next number or symbol token from source until
const u8 *NextNumberOrSymbolToken(const u8 *source, struct Token *result);
// Reads the next string token from source
const u8 *NextStringToken(const u8 *source, struct Token *result);
// Reads the next line comment token from source
const u8 *NextLineCommentToken(const u8 *source, struct Token *result);

// Reads all initial whitespace from source and discards it.
const u8 *DiscardInitialWhitespace(const u8 *source);
// If an escape is encountered, extends token with it and the next character.
const u8 *HandleEscape(const u8 *source, struct Token *result);

// Consumes 0+ digits and returns the number encountered.
u64 CountDigits(const u8 **source, u64 *length);
// Consumes an optional number sign.
void OptionalNumberSign(const u8 **source, u64 *length);
// Consumes a single character
void ConsumeChar(const u8 **source, u64 *length);

// Disambiguates between Integer, Real32, Real64, and symbol tokens.
enum TokenType NumberOrSymbolTokenType(const u8 *source, u64 length);

// True if source is pointing at: decimal-point digit* [exponent]
// if at_least_one_decimal_digit is true, then source must be pointing at:
//   decimal-point digit+ [exponent] | decimal-point exponent
b64 IsDecimalAndOptionalExponent(const u8 **source, u64 *length, u8 *exponent_marker, b64 at_least_one_decimal_digit);
// True if source is pointing at an exponent.
// Exponent-marker is filled with the Real32 or Real64 exponent marker if true.
b64 IsExponent(const u8 **source, u64 *length, u8 *exponent_marker);

// True if source is pointing at an integer.
b64 IsInteger(const u8 *source, u64 length);
// True if source is pointing at a Real.
// exponent_marker is filled with the exponent marker of the real (defaulting to 'e').
b64 IsReal(const u8 *source, u64 length, u8 *exponent_marker);

// True if c is a whitespace character
b64 IsWhitespace(u8 c);
// True if c is a close delimiter for a symbol/number
b64 IsSymbolCloseDelimiter(u8 c);
// True if marker is a Real32 exponent marker
b64 IsReal32ExponentMarker(u8 marker);
// True if marker is a Real exponent marker
b64 IsExponentMarker(u8 marker);
// True if c is a number sign
b64 IsNumberSign(u8 c);
// True if c is a digit
b64 IsDigit(u8 c);

// Result is set to be a single character token. Returns remaining source.
const u8 *SingleCharacterToken(enum TokenType type, const u8 *source, struct Token *result);
// Result is set to be a 0-character eof token. Returns remaining source.
const u8 *EOFToken(const u8 *source, struct Token *result);
// token is extended to have another character. Returns remaining source.
const u8 *ExtendToken(const u8 *source, struct Token *token);
// Returns source after discarding a character.
const u8 *DiscardChar(const u8 *source);



const u8 *NextToken(const u8 *source, struct Token *result) {
  source = DiscardInitialWhitespace(source);
  u8 next = *source;
  // Single character tokens
  if (next ==  '(')
    return SingleCharacterToken(TOKEN_LIST_OPEN, source, result);
  else if (next ==  ')')
    return SingleCharacterToken(TOKEN_LIST_CLOSE, source, result);
  else if (next == '\'')
    return SingleCharacterToken(TOKEN_SYMBOLIC_QUOTE, source, result);
  else if (next ==  '.' && IsSymbolCloseDelimiter(*DiscardChar(source)))
    return SingleCharacterToken(TOKEN_PAIR_SEPARATOR, source, result);
  else if (next == '\0')
    return EOFToken(source, result);

  // Multi-character tokens
  else if (next ==  '"')
    return NextStringToken(source, result);
  else if (next ==  ';')
    return NextLineCommentToken(source, result);
  else
    return NextNumberOrSymbolToken(source, result);
}

u8 *CopyTokenSource(const struct Token *token) {
  u8 *source = malloc(token->length+1);
  strncpy(source, token->source, token->length);
  source[token->length] = '\0';
  return source;
}

void PrintToken(struct Token* token) {
  u8 *source = CopyTokenSource(token);
  printf("Token type=%s, length=%llu, source:\"%s\"\n", TokenTypeString(token->type), token->length, source);
  free(source);
}

const u8 *TokenTypeString(enum TokenType type) {
  switch (type) {
    case TOKEN_LIST_OPEN: return "LIST_OPEN";
    case TOKEN_LIST_CLOSE: return "LIST_CLOSE";
    case TOKEN_PAIR_SEPARATOR: return "PAIR_SEPARATOR";
    case TOKEN_SYMBOLIC_QUOTE: return "SYMBOLIC_QUOTE";
    case TOKEN_STRING: return "STRING";
    case TOKEN_INTEGER: return "INTEGER";
    case TOKEN_REAL32: return "REAL32";
    case TOKEN_REAL64: return "REAL64";
    case TOKEN_SYMBOL: return "SYMBOL";
    case TOKEN_LINE_COMMENT: return "LINE_COMMENT";
    case TOKEN_EOF: return "EOF";
    case TOKEN_UNTERMINATED_STRING: return "UNTERMINATED_STRING";
  }
}




enum TokenType NumberOrSymbolTokenType(const u8 *source, u64 length) {
  u8 exponent_marker;
  if (IsInteger(source, length)) {
    return TOKEN_INTEGER;
  } else if (IsReal(source, length, &exponent_marker)) {
    if (IsReal32ExponentMarker(exponent_marker)) {
      return TOKEN_REAL32;
    } else {
      return TOKEN_REAL64;
    }
  } else {
    return TOKEN_SYMBOL;
  }
}

const u8 *NextNumberOrSymbolToken(const u8 *source, struct Token *result) {
  // Read the full token
  result->source = source;
  result->length = 0;
  // Token is complete when we encounter a close delimiter
  for (; !IsSymbolCloseDelimiter(*source); source = ExtendToken(source, result))
    source = HandleEscape(source, result);

  // Determine if integer, real32, real64, or symbol
  result->type = NumberOrSymbolTokenType(result->source, result->length);
  return source;
}

const u8 *NextStringToken(const u8 *source, struct Token *result) {
  source = DiscardChar(source); // Discard "
  result->type = TOKEN_STRING;
  result->source = source;
  result->length = 0;

  for (; *source != '"'; source = ExtendToken(source, result)) {
    source = HandleEscape(source, result);
    if (*source == 0) {
      result->type = TOKEN_UNTERMINATED_STRING;
      return source;
    }
  }
  return DiscardChar(source); // Discard "
}

const u8 *NextLineCommentToken(const u8 *source, struct Token *result) {
  result->type = TOKEN_LINE_COMMENT;
  result->source = source;
  result->length = 0;
  for (; *source != '\0' && *source != '\n'; source = ExtendToken(source, result))
    ;
  return source;
}

const u8 *SingleCharacterToken(enum TokenType type, const u8 *source, struct Token *result) {
  result->type = type;
  result->source = source;
  result->length = 1;
  return source + 1;
}

const u8 *EOFToken(const u8 *source, struct Token *result) {
  result->type = TOKEN_EOF;
  result->source = source;
  result->length = 0;
  return source;
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


b64 IsWhitespace(u8 ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f'; }
// Symbols end at "()'; whitespace, or eof
// Symbols CAN contain .
b64 IsSymbolCloseDelimiter(u8 ch) {
  return IsWhitespace(ch) || ch == '(' || ch == ')' || ch == '\0' || ch == '\'' || ch == '"' || ch == ';';
}
b64 IsDigit(u8 c) { return c >= '0' && c <= '9'; }
b64 IsNumberSign(u8 c) { return c == '+' || c == '-'; }
b64 IsReal32ExponentMarker(u8 marker) {
  return marker == 's' || marker == 'S';
}
b64 IsExponentMarker(u8 marker) {
  return IsReal32ExponentMarker(marker) || marker == 'e' || marker == 'E' || marker == 'd' || marker == 'D';
}


const u8 *DiscardInitialWhitespace(const u8 *source) {
  for (; IsWhitespace(*source); source = DiscardChar(source));
  return source;
}

const u8 *HandleEscape(const u8 *source, struct Token *result) {
  if (*source == '\\') {
    return ExtendToken(source, result);
  }
  return source;
}

const u8 *ExtendToken(const u8 *source, struct Token *token) {
  token->length++;
  return DiscardChar(source);
}

const u8 *DiscardChar(const u8 *source) {
  return source + 1;
}

void TestToken() {
#define S(str) str, strlen(str)
  assert(!IsInteger(S("hello")));
  assert(IsInteger(S("123")));
  assert(IsInteger(S("+123")));
  assert(IsInteger(S("-123")));
  assert(!IsInteger(S("- 123")));
  assert(!IsInteger(S("+12 3")));

  u8 exponent_marker;
  // real := [sign] {digit}*    decimal-point {digit}*   [exponent]
  //       | [sign] {digit}+  [ decimal-point {digit}* ]  exponent

  assert(IsReal(S("1."), &exponent_marker));
  assert(IsReal(S("+1."), &exponent_marker));
  assert(IsReal(S("-1239."), &exponent_marker));
  assert(IsReal(S(".0"), &exponent_marker));
  assert(IsReal(S("+.123"), &exponent_marker));
  assert(IsReal(S("+987.123"), &exponent_marker));
  assert(IsReal(S("+987.123e1"), &exponent_marker));
  assert(IsReal(S("+987.123s-0"), &exponent_marker));
  assert(IsReal32ExponentMarker(exponent_marker));
  assert(IsReal(S("+987.123D+234"), &exponent_marker));
  assert(IsExponentMarker(exponent_marker));

  assert(IsReal(S("0e1"), &exponent_marker));
  assert(IsReal(S("234S+23"), &exponent_marker));
  assert(IsReal(S("+888d-789"), &exponent_marker));
  assert(IsReal(S("-890E789"), &exponent_marker));

  assert(!IsReal(S(".e"), &exponent_marker));
  assert(!IsReal(S(".s-"), &exponent_marker));
  assert(!IsReal(S("+.d"), &exponent_marker));
  assert(!IsReal(S("-.e+"), &exponent_marker));
  assert(!IsReal(S("-1f7"), &exponent_marker));
  assert(!IsReal(S("-1d+"), &exponent_marker));
  assert(!IsReal(S("-1d+3junk"), &exponent_marker));
  assert(!IsReal(S("-.0junk"), &exponent_marker));
  assert(!IsReal(S("+.0ejunk"), &exponent_marker));
  assert(!IsReal(S(".0e+junk"), &exponent_marker));

  assert(!IsReal(S("."), &exponent_marker));
  assert(!IsReal(S("+."), &exponent_marker));
  assert(!IsReal(S("-."), &exponent_marker));
#undef S

  const u8 *source = "(the (quick 'brown) fox \"jumped ;)(\\\" over the \" ;; I'm a line comment\n lazy) 123 1.234 1s0 dog (car . cdr) last #t #f";
  enum TokenType expected_types[] = {
    TOKEN_LIST_OPEN,
    TOKEN_SYMBOL,
    TOKEN_LIST_OPEN,
    TOKEN_SYMBOL,
    TOKEN_SYMBOLIC_QUOTE,
    TOKEN_SYMBOL,
    TOKEN_LIST_CLOSE,
    TOKEN_SYMBOL,
    TOKEN_STRING,
    TOKEN_LINE_COMMENT,
    TOKEN_SYMBOL,
    TOKEN_LIST_CLOSE,
    TOKEN_INTEGER,
    TOKEN_REAL64,
    TOKEN_REAL32,
    TOKEN_SYMBOL,
    TOKEN_LIST_OPEN,
    TOKEN_SYMBOL,
    TOKEN_PAIR_SEPARATOR,
    TOKEN_SYMBOL,
    TOKEN_LIST_CLOSE,
    TOKEN_SYMBOL,
    TOKEN_SYMBOL,
    TOKEN_SYMBOL,
    TOKEN_EOF,
  };

  struct Token token;
  int i = 0;
  for (source = NextToken(source, &token);
      token.type != TOKEN_EOF && token.type != TOKEN_UNTERMINATED_STRING;
      source = NextToken(source, &token), ++i) {
    assert(token.type == expected_types[i]);
  }
  assert(token.type == expected_types[i]);

  source = "\" I'm an unterminated string";
  NextToken(source, &token);
  assert(token.type == TOKEN_UNTERMINATED_STRING);

  source = ";; I'm a line comment";
  source = NextToken(source, &token);
  assert(token.type == TOKEN_LINE_COMMENT);
  NextToken(source, &token);
  assert(token.type == TOKEN_EOF);

  source = "symbol\\)-with\\ escaped";
  source = NextToken(source, &token);
  assert(token.type == TOKEN_SYMBOL);
  NextToken(source, &token);
  assert(token.type == TOKEN_EOF);

  source = "symbol\"string\"";
  source = NextToken(source, &token);
  assert(token.type == TOKEN_SYMBOL);
  source = NextToken(source, &token);
  assert(token.type == TOKEN_STRING);
  NextToken(source, &token);
  assert(token.type == TOKEN_EOF);
}

int main(int argc, char **argv) {
  TestToken();
  return 0;
}
