#include "parse.h"

#include <assert.h>
#include <stdio.h>

#include "tag.h"
#include "memory.h"
#include "symbol_table.h"
#include "rope.h"


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

struct ParseState {
  int last_char;
  b64 eof;
  u64 chars;
  u64 lines;
  u64 sub_forms;
};

struct StringStream {
  const char *string;
  u64 position;
};

enum StreamType {
  STREAM_TYPE_FILE,
  STREAM_TYPE_STRING,
};
struct Stream {
  enum StreamType type;
  union {
    struct StringStream string_stream;
    FILE *file;
  } as;
};

int GetChar(struct Stream *stream) {
  if (stream->type == STREAM_TYPE_FILE) {
    return fgetc(stream->as.file);
  } else if (stream->type == STREAM_TYPE_STRING) {
    struct StringStream *ss = &stream->as.string_stream;
    char value = ss->string[ss->position++];
    // the end of the string is considered EOF.
    if (!value) return EOF;
    return value;
  } else {
    assert(!"Invalid stream type");
  }
}

struct Stream MakeStringStream(const char *string) {
  struct Stream stream;
  stream.type = STREAM_TYPE_STRING;
  stream.as.string_stream.position = 0;
  stream.as.string_stream.string = string;
  return stream;
}

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

struct ParseState MakeParseState();
int ReadChar(struct Stream *stream, struct ParseState* state);
int PeekChar(struct Stream *stream, struct ParseState* state);

int ReadCharIgnoringWhitespace(struct Stream *stream, struct ParseState *state);
Object ReadObject(struct Memory *memory, struct Stream *stream, struct ParseState *state);
Object ReadQuote(struct Memory *memory, struct Stream *stream, struct ParseState *state);
Object ReadString(struct Memory *memory, struct Stream *stream, struct ParseState *state);
Object ReadHash(struct Memory *memory, struct Stream *stream, struct ParseState *state);
Object ReadNumberOrSymbol(struct Memory *memory, struct Stream *stream, struct ParseState *state);
Object ReadFloatingPointOrSymbol(struct Memory *memory, struct Stream *stream, struct ParseState *state);
Object ReadFloatingPoint2OrSymbol(struct Memory *memory, struct Stream *stream, struct ParseState *state);
Object ReadListLike(struct Memory *memory, struct Stream *stream, struct ParseState *state);
Object ReadSymbol(struct Memory *memory, struct Stream *stream, struct ParseState *state);

struct ParseState MakeParseState() {
  struct ParseState state;
  state.last_char = 0;
  state.eof = 0;
  state.chars = 0;
  state.lines = 0;
  state.sub_forms = 0;
  return state;
}

int ReadChar(struct Stream *stream, struct ParseState* state) {
  int next = GetChar(stream);
  state->last_char = next;
  ++state->chars;
  if (next == EOF)
    state->eof = 1;
  if (next == '\n')
    ++state->lines;
  return next;
}

Object ReadObject(struct Memory *memory, struct Stream *stream, struct ParseState *state) {
  int next = ReadCharIgnoringWhitespace(stream, state);
  if (IsEof(next)) {
    return nil;
  } else if (IsQuote(next)) {
    return ReadQuote(memory, stream, state);
  } else if (IsDoubleQuote(next)) {
    return ReadString(memory, stream, state);
  } else if (IsHash(next)) {
    return ReadHash(memory, stream, state);
  } else if (IsSignChar(next) || IsDigitChar(next)) {
    return ReadNumberOrSymbol(memory, stream, state);
  } else if (IsDecimalPoint(next)) {
    return ReadFloatingPointOrSymbol(memory, stream, state);
  } else if (IsExponentMarker(next)) {
    return ReadFloatingPoint2OrSymbol(memory, stream, state);
  } else if (IsOpenParen(next)) {
    return ReadListLike(memory, stream, state);
  } else {
    return ReadSymbol(memory, stream, state);
  }
  ++state->sub_forms;
}

int ReadCharIgnoringWhitespace(struct Stream *stream, struct ParseState *state) {
  int next = ReadChar(stream, state);
  while (IsWhitespace(next))
    next = ReadChar(stream, state);
  return next;
}

Object ReadQuote(struct Memory *memory, struct Stream *stream, struct ParseState *state) { return nil; }
Object ReadString(struct Memory *memory, struct Stream *stream, struct ParseState *state) { return nil; }
Object ReadHash(struct Memory *memory, struct Stream *stream, struct ParseState *state) { return nil; }
Object ReadNumberOrSymbol(struct Memory *memory, struct Stream *stream, struct ParseState *state) { return nil; }
Object ReadFloatingPointOrSymbol(struct Memory *memory, struct Stream *stream, struct ParseState *state) { return nil; }
Object ReadFloatingPoint2OrSymbol(struct Memory *memory, struct Stream *stream, struct ParseState *state) { return nil; }
Object ReadListLike(struct Memory *memory, struct Stream *stream, struct ParseState *state) { return nil; }

Object ReadSymbol(struct Memory *memory, struct Stream *stream, struct ParseState *state) {
  return nil;
}

void TestParse() {
  struct Memory memory = InitializeMemory(128, 13);
  struct Stream stream = MakeStringStream("   symbol  ");
  struct ParseState state = MakeParseState();
  Object object = ReadObject(&memory, &stream, &state);
}

int main(int argc, char** argv) {
  TestTag();
  TestMemory();
  TestSymbolTable();
  TestRope();
  //TestParse();
}
