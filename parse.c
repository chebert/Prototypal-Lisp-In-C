#include "parse.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "pair.h"
#include "read_buffer.h"
#include "root.h"
#include "string.h"
#include "symbol_table.h"
#include "tag.h"


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
    if (value == 0) return EOF;
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

b64 IsNumberSign(int ch) { return ch == '+' || ch == '-'; }
b64 IsDigit(int ch) {return ch >= '0' && ch <= '9'; }
b64 IsDecimalPoint(int ch) { return ch == '.'; }
b64 IsExponentMarker(int ch) { return ch == 'e' || ch == 'E' || ch == 'f' || ch == 'F' || ch == 'd' || ch == 'D'; }

b64 IsSymbolicQuote(int ch) { return ch == '\''; }
b64 IsStringQuote(int ch) { return ch == '"'; }

b64 IsHash(int ch) { return ch == '#'; }

b64 IsOpenList(int ch) { return ch == '('; }
b64 IsCloseList(int ch) { return ch == ')'; }

b64 IsWhitespace(int ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f'; }
b64 IsEof(int ch) { return ch == EOF; }
b64 IsEscape(int ch) { return ch == '\\'; }

struct ParseState MakeParseState();
int ReadChar(struct Stream *stream, struct ParseState* state);
int PeekChar(struct Stream *stream, struct ParseState* state);

int ReadCharIgnoringWhitespace(struct Stream *stream, struct ParseState *state);
Object ReadObject(struct Stream *stream, struct ParseState *state);
Object ReadQuote(struct Stream *stream, struct ParseState *state);
Object ReadString(struct Stream *stream, struct ParseState *state);
Object ReadHash(struct Stream *stream, struct ParseState *state);
Object ReadSignedNumberOrSymbol(struct Stream *stream, struct ParseState *state);
Object ReadNumberOrSymbol(struct Stream *stream, struct ParseState *state);
Object ReadFloatingPointOrSymbol(struct Stream *stream, struct ParseState *state);
Object ReadListLike(struct Stream *stream, struct ParseState *state);
Object ReadSymbol(struct Stream *stream, struct ParseState *state);

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

Object ReadObject(struct Stream *stream, struct ParseState *state) {
  int next = ReadCharIgnoringWhitespace(stream, state);
  if (IsEof(next)) {
    return nil;
  } else if (IsSymbolicQuote(next)) {
    return ReadQuote(stream, state);
  } else if (IsStringQuote(next)) {
    return ReadString(stream, state);
  } else if (IsHash(next)) {
    return ReadHash(stream, state);
  } else if (IsNumberSign(next)) {
    return ReadSignedNumberOrSymbol(stream, state);
  } else if (IsDigit(next)) {
    return ReadNumberOrSymbol(stream, state);
  } else if (IsDecimalPoint(next)) {
    return ReadFloatingPointOrSymbol(stream, state);
  } else if (IsOpenList(next)) {
    return ReadListLike(stream, state);
  } else {
    return ReadSymbol(stream, state);
  }
  ++state->sub_forms;
}

int ReadCharIgnoringWhitespace(struct Stream *stream, struct ParseState *state) {
  int next = ReadChar(stream, state);
  while (IsWhitespace(next))
    next = ReadChar(stream, state);
  return next;
}

Object ReadSignedNumberOrSymbol(struct Stream *stream, struct ParseState *state) { return nil; }
Object ReadNumberOrSymbol(struct Stream *stream, struct ParseState *state) { return nil; }
Object ReadFloatingPointOrSymbol(struct Stream *stream, struct ParseState *state) { return nil; }

Object ReadQuote(struct Stream *stream, struct ParseState *state) {
  Object object = ReadObject(stream, state);
  SetRegister(REGISTER_READ_BUFFER, MakePair(object, nil));
  // REFERENCES INVALIDATED

  Object symbol = InternSymbol(AllocateString("quote"));
  // REFERENCES INVALIDATED

  Object quoted_object = MakePair(symbol, GetRegister(REGISTER_READ_BUFFER));
  return quoted_object;
}
Object ReadListLike(struct Stream *stream, struct ParseState *state) { return nil; }

int HandleEscape(struct Stream *stream, struct ParseState *state, int next) {
  if (IsEscape(next)) {
    next = ReadChar(stream, state);
    assert(!IsEof(next));
  }
  return next;
}

Object ReadString(struct Stream *stream, struct ParseState *state) {
  ResetReadBuffer(256);
  for (int next = ReadChar(stream, state);
      !(IsStringQuote(next) || IsEof(next));
      next = ReadChar(stream, state)) {
    AppendReadBuffer(HandleEscape(stream, state, next));
  }
  return FinalizeReadBuffer();
}

Object ReadSymbol(struct Stream *stream, struct ParseState *state) {
  ResetReadBuffer(256);
  for (int next = state->last_char;
      !(IsCloseList(next) || IsWhitespace(next) || IsEof(next));
      next = ReadChar(stream, state)) {
    AppendReadBuffer(HandleEscape(stream, state, next));
  }
  Object name = FinalizeReadBuffer();
  return InternSymbol(name);
}

Object ReadHash(struct Stream *stream, struct ParseState *state) {
  ResetReadBuffer(256);

  ReadChar(stream, state); // Discard the # when reading the symbol
  Object symbol = ReadSymbol(stream, state);
  u8 *string = StringCharacterBuffer(symbol);

  if (!strcmp(string, "f")) return false;
  if (!strcmp(string, "t")) return true;

  printf("Invalid hash #%s\n", string);
  assert(!"Invalid hash");
}

void TestParse() {
  InitializeMemory(128);
  InitializeSymbolTable(13);
  // symbol
  struct Stream stream = MakeStringStream("   symbol  ");
  struct ParseState state = MakeParseState();
  Object object = ReadObject(&stream, &state);
  PrintlnObject(object);

  // "Hello \"world\""
  stream = MakeStringStream("   \"Hello \\\"world\\\"\"  ");
  state = MakeParseState();
  object = ReadObject(&stream, &state);
  PrintlnObject(object);

  stream = MakeStringStream("  #f ");
  state = MakeParseState();
  object = ReadObject(&stream, &state);
  PrintlnObject(object);

  stream = MakeStringStream("  #t ");
  state = MakeParseState();
  object = ReadObject(&stream, &state);
  PrintlnObject(object);

  stream = MakeStringStream(" 'symbol ");
  state = MakeParseState();
  object = ReadObject(&stream, &state);
  PrintlnObject(object);
}

int main(int argc, char** argv) {
  TestTag();
  TestMemory();
  TestSymbolTable();
  TestReadBuffer();
  TestParse();
}
