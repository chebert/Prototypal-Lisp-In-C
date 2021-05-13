#include <stdio.h>

#include "tag.h"
#include "memory.h"
#include "symbol_table.h"

#include "parse.h"

// Object := floating-point
//         | integer
//         | nil
//         | true
//         | false
//         | string
//         | list
//         | pair
//         | quote
//         | symbol

// floating-point  := [sign] {digit}*  decimal-point {digit}*  [exponent]
//                  | [sign] {digit}+ [decimal-point {digit}*]  exponent
// sign            := + | -
// digit           := 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9
// decimal-point   := .
// exponent        := exponent-marker [sign] {digit+}
// exponent-marker := e | E | f | F | d | D

// integer := [sign] {digit}+

// nil   := ( {whitespace*} )
// true  := #t
// false := #f

// string      := double-quote {string-char}* double-quote
// string-char := [^{double-quote}] | escaped-character
// not-double-quote := /^"/
// escaped-character := \ {character}

// list := ( {whitespace}* Object {{whitespace}+ Object}* {whitespace*})
// pair := ( {whitespace}* {Object} {whitespace}* . {whitespace}* {Object} {whitespace}* )

// quote := ' {whitespace}* {Object}

// symbol := {first-symbol-character}{symbol-character}+ | pipe {escaped-character}* pipe
// symbol-character       := [^"(){whitespace}] | escaped-character
// first-symbol-character := [^#"(){whitespace}] | escaped-character
// escaped-character      := [^{pipe}] | escaped-character
// pipe := |

static struct Memory memory;

b64 IsSignChar(int ch) { return ch == '+' || ch == '-'; }
b64 IsDigitChar(int ch) {return ch >= '0' && ch <= '9'; }
b64 IsQuote(int ch) { return ch == '\''; }
b64 IsDoubleQuote(int ch) { return ch == '"'; }
b64 IsHash(int ch) { return ch == '#'; }
b64 IsOpenParen(int ch) { return ch == '('; }
b64 IsCloseParen(int ch) { return ch == '('; }
b64 IsDecimalPoint(int ch) { return ch == '.'; }
b64 IsExponentMarker(int ch) { return ch == 'e' || ch == 'E' || ch == 'f' || ch == 'F' || ch == 'd' || ch == 'D'; }
b64 IsEof(int ch) { return ch == EOF; }
b64 IsWhitespace(int ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f'; }

struct ParseState {
  int last_char;
  b64 eof;
  u64 chars;
  u64 lines;
  u64 sub_forms;
};

struct ParseState MakeParseState();
int ReadChar(FILE *stream, struct ParseState* state);
int PeekChar(FILE *stream, struct ParseState* state);

int ReadCharIgnoringWhitespace(FILE *stream, struct ParseState *state);
Object ReadObject(FILE *stream, struct ParseState *state);
Object ReadQuote(FILE *stream, struct ParseState *state);
Object ReadString(FILE *stream, struct ParseState *state);
Object ReadHash(FILE *stream, struct ParseState *state);
Object ReadNumberOrSymbol(FILE *stream, struct ParseState *state);
Object ReadFloatingPointOrSymbol(FILE *stream, struct ParseState *state);
Object ReadFloatingPoint2OrSymbol(FILE *stream, struct ParseState *state);
Object ReadListLike(FILE *stream, struct ParseState *state);
Object ReadSymbol(FILE *stream, struct ParseState *state);

struct ParseState MakeParseState() {
  struct ParseState state;
  state.last_char = 0;
  state.eof = 0;
  state.chars = 0;
  state.lines = 0;
  state.sub_forms = 0;
  return state;
}

int ReadChar(FILE *stream, struct ParseState* state) {
  int next = fgetc(stream);
  state->last_char = next;
  ++state->chars;
  if (next == EOF)
    state->eof = 1;
  if (next == '\n')
    ++state->lines;
  return next;
}

Object ReadObject(FILE *stream, struct ParseState *state) {
  int next = ReadCharIgnoringWhitespace(stream, state);
  if (IsEof(next)) {
    return nil;
  } else if (IsQuote(next)) {
    return ReadQuote(stream, state);
  } else if (IsDoubleQuote(next)) {
    return ReadString(stream, state);
  } else if (IsHash(next)) {
    return ReadHash(stream, state);
  } else if (IsSignChar(next) || IsDigitChar(next)) {
    return ReadNumberOrSymbol(stream, state);
  } else if (IsDecimalPoint(next)) {
    return ReadFloatingPointOrSymbol(stream, state);
  } else if (IsExponentMarker(next)) {
    return ReadFloatingPoint2OrSymbol(stream, state);
  } else if (IsOpenParen(next)) {
    return ReadListLike(stream, state);
  } else {
    return ReadSymbol(stream, state);
  }
  ++state->sub_forms;
}

int ReadCharIgnoringWhitespace(FILE *stream, struct ParseState *state) {
  int next = ReadChar(stream, state);
  while (IsWhitespace(next))
    next = ReadChar(stream, state);
  return next;
}

Object ReadQuote(FILE *stream, struct ParseState *state) {}
Object ReadString(FILE *stream, struct ParseState *state) {}
Object ReadHash(FILE *stream, struct ParseState *state) {}
Object ReadNumberOrSymbol(FILE *stream, struct ParseState *state) {}
Object ReadFloatingPointOrSymbol(FILE *stream, struct ParseState *state) {}
Object ReadFloatingPoint2OrSymbol(FILE *stream, struct ParseState *state) {}
Object ReadListLike(FILE *stream, struct ParseState *state) {}
Object ReadSymbol(FILE *stream, struct ParseState *state) {}

int main(int argc, char** argv) {
  //TestTag();
  //TestMemory();
  TestSymbolTable();
  //memory = AllocateMemory(128);
}
