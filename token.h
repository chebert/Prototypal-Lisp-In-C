#ifndef TOKEN_H
#define TOKEN_H

#include "c_types.h"

enum TokenType {
  // Token representing the beginning of a list: (
  TOKEN_LIST_OPEN,
  // Token representing the end of a list: )
  TOKEN_LIST_CLOSE,

  // Token representing a pair of two objects: e.g. the . in (first second . rest)
  TOKEN_PAIR_SEPARATOR,

  // Token representing a symbolic-quote: '(quoted object)
  TOKEN_SYMBOLIC_QUOTE,

  // Token representing a text quote:  "string"
  TOKEN_STRING,
  // Token representing an integer: -1337
  TOKEN_INTEGER,
  // Token representing a 32-bit floating point number: 3.4s0
  TOKEN_REAL32,
  // Token representing a 64-bit floating point number: .876e203
  TOKEN_REAL64,
  // Token representing a symbol
  TOKEN_SYMBOL,

  // Token representing a line comment beginning with ;
  TOKEN_LINE_COMMENT,

  // Token representing end-of-file
  TOKEN_EOF,
  // Error token created when an EOF is encountered before a string is terminated.
  TOKEN_UNTERMINATED_STRING
};

// Return the string representing the token type.
const u8 *TokenTypeString(enum TokenType type);

struct Token {
  // Type of the token.
  enum TokenType type;
  // Pointer to the start of the token in the source string. Not null-terminated.
  const u8* source;
  // Length (in characters) of the token.
  u64 length;
};

// Reads from source. The next token is stored in result.
// The unconsumed portion of source returned.
const u8 *NextToken(const u8 *source, struct Token *result);

// Returns a NULL-terminated copy of the token source string.
// resulting string should be free()'d.
u8 *CopyTokenSource(const struct Token *token);

// Debug print of token
void PrintToken(struct Token* token);

void TestToken();

#endif
