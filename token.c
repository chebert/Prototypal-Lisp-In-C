#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c_types.h"

enum TokenType {
  TOKEN_LIST_OPEN,
  TOKEN_LIST_CLOSE,

  TOKEN_PAIR_SEPARATOR,

  TOKEN_SYMBOLIC_QUOTE,

  TOKEN_STRING,
  TOKEN_INTEGER,
  TOKEN_REAL32,
  TOKEN_REAL64,
  TOKEN_SYMBOL,

  TOKEN_LINE_COMMENT,

  TOKEN_EOF,
  TOKEN_UNTERIMINATED_STRING
};

struct Token {
  enum TokenType type;
  const u8* source;
  u64 length;
};
void PrintToken(struct Token* token);
const u8 *TokenTypeString(enum TokenType type);

// Reads from source. The next token is stored in result.
// The unconsumed portion of source returned.
const u8 *NextToken(const u8 *source, struct Token *result);

const u8 *NextNumberOrSymbolOrPairSeparatorToken(const u8 *source, struct Token *result);
const u8 *NextNumberOrSymbolToken(const u8 *source, struct Token *result);
const u8 *NextStringToken(const u8 *source, struct Token *result);
const u8 *NextLineCommentToken(const u8 *source, struct Token *result);
const u8 *DiscardInitialWhitespace(const u8 *source);
const u8 *HandleEscape(const u8 *source, struct Token *result);

b64 IsInteger(const u8 *source, u64 length);
b64 IsReal(const u8 *source, u64 length, u8 *exponent_marker);

b64 IsWhitespace(u8 c);
b64 IsCloseDelimiter(u8 c);
b64 IsReal32ExponentMarker(u8 marker);
b64 IsExponentMarker(u8 marker);
b64 IsNumberSign(u8 c);
b64 IsDigit(u8 c);

struct Token SingleCharacterToken(enum TokenType type, const u8 *source);
const u8 *ExtendToken(const u8 *source, struct Token *token);
const u8 *DiscardChar(const u8 *source);

const u8 *NextToken(const u8 *source, struct Token *result) {
  source = DiscardInitialWhitespace(source);
  u8 next = *source;
  // Single character tokens
  if      (next ==  '(')
    *result = SingleCharacterToken(TOKEN_LIST_OPEN,      source++);
  else if (next ==  ')')
    *result = SingleCharacterToken(TOKEN_LIST_CLOSE,     source++);
  else if (next == '\'')
    *result = SingleCharacterToken(TOKEN_SYMBOLIC_QUOTE, source++);
  else if (next == '\0')
    *result = SingleCharacterToken(TOKEN_EOF,            source);
  else if (next ==  '.' && IsCloseDelimiter(*DiscardChar(source)))
    *result = SingleCharacterToken(TOKEN_PAIR_SEPARATOR, source++);

  // Multi-character tokens
  else if (next ==  '"') source =        NextStringToken(source, result);
  else if (next ==  ';') source =    NextLineCommentToken(source, result);
  else                   source = NextNumberOrSymbolToken(source, result);
  return source;
}

const u8 *HandleEscape(const u8 *source, struct Token *result) {
  if (*source == '\\') {
    return ExtendToken(source, result);
  }
  return source;
}

enum TokenType NumberOrSymbolTokenType(const u8 *source, u64 length) {
  u8 exponent_marker = 'e';
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
  for (; !IsCloseDelimiter(*source); source = ExtendToken(source, result))
    ;

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
      result->type = TOKEN_UNTERIMINATED_STRING;
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

void PrintToken(struct Token* token) {
  u8 *source = (u8 *)malloc(token->length+1);
  strncpy(source, token->source, token->length);
  source[token->length] = '\0';

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
    case TOKEN_UNTERIMINATED_STRING: return "UNTERMINATED_STRING";
  }
}

struct Token SingleCharacterToken(enum TokenType type, const u8 *source) {
  struct Token token;
  token.type = type;
  token.source = source;
  token.length = 1;
  return token;
}

b64 IsWhitespace(u8 ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f'; }
b64 IsCloseDelimiter(u8 ch) { return IsWhitespace(ch) || ch == ')' || ch == '\0' || ch == ';'; }

const u8 *DiscardInitialWhitespace(const u8 *source) {
  for (; IsWhitespace(*source); source = DiscardChar(source));
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

b64 IsDigit(u8 c) { return c >= '0' && c <= '9'; }
b64 IsNumberSign(u8 c) { return c == '+' || c == '-'; }
b64 IsReal32ExponentMarker(u8 marker) {
  return marker == 's' || marker == 'S';
}
b64 IsExponentMarker(u8 marker) {
  return IsReal32ExponentMarker(marker) || marker == 'e' || marker == 'E' || marker == 'd' || marker == 'D';
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
  struct Token token;
  for (source = NextToken(source, &token);
      token.type != TOKEN_EOF && token.type != TOKEN_UNTERIMINATED_STRING;
      source = NextToken(source, &token)) {
    PrintToken(&token);
  }
  PrintToken(&token);

  source = "\" I'm an unterminated string";
  for (source = NextToken(source, &token);
      token.type != TOKEN_EOF && token.type != TOKEN_UNTERIMINATED_STRING;
      source = NextToken(source, &token)) {
    PrintToken(&token);
  }
  PrintToken(&token);

  source = ";; I'm an unterminated comment";
  for (source = NextToken(source, &token);
      token.type != TOKEN_EOF && token.type != TOKEN_UNTERIMINATED_STRING;
      source = NextToken(source, &token)) {
    PrintToken(&token);
  }
  PrintToken(&token);
}

int main(int argc, char **argv) {
  TestToken();
  return 0;
}
