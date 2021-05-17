#include "parse.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "byte_vector.h"
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
// exponent-marker := e | E | s | S | d | D

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

struct ParseState {
  int last_char;
  b64 eof;
  u64 chars;
  u64 lines;
  u64 sub_forms;
  struct Stream stream;
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
b64 IsExponentMarker(int ch) { return ch == 'e' || ch == 'E' || ch == 's' || ch == 'S' || ch == 'd' || ch == 'D'; }

b64 IsSymbolicQuote(int ch) { return ch == '\''; }
b64 IsStringQuote(int ch) { return ch == '"'; }

b64 IsHash(int ch) { return ch == '#'; }

b64 IsOpenList(int ch) { return ch == '('; }
b64 IsCloseList(int ch) { return ch == ')'; }

b64 IsWhitespace(int ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f'; }
b64 IsEof(int ch) { return ch == EOF; }
b64 IsEscape(int ch) { return ch == '\\'; }

struct ParseState MakeParseState(struct Stream stream);
int ReadChar(struct ParseState* state);
int PeekChar(struct ParseState* state);
void PrintParseState(struct ParseState *state);

int ReadCharIgnoringWhitespace(struct ParseState *state);
Object ReadObject(struct ParseState *state);
Object ReadQuote(struct ParseState *state);
Object ReadString(struct ParseState *state);
Object ReadHash(struct ParseState *state);
Object ReadSignedNumberOrSymbol(struct ParseState *state);
Object ReadNumberOrSymbol(struct ParseState *state);
Object ReadFloatingPointOrSymbol(struct ParseState *state);
Object ReadExponentOrSymbol(struct ParseState *state);
Object ReadListLike(struct ParseState *state);
Object ReadSymbol(struct ParseState *state);

// Returns the next character.
int HandleEscape(struct ParseState *state, int next);

struct ParseState MakeParseState(struct Stream stream) {
  struct ParseState state;
  state.last_char = 0;
  state.eof = 0;
  state.chars = 0;
  state.lines = 0;
  state.sub_forms = 0;
  state.stream = stream;
  return state;
}

int ReadChar(struct ParseState* state) {
  int next = GetChar(&state->stream);
  state->last_char = next;
  ++state->chars;
  if (next == EOF)
    state->eof = 1;
  if (next == '\n')
    ++state->lines;
  return next;
}

Object ReadObject(struct ParseState *state) {
  ResetReadBuffer(256);
  int next = ReadCharIgnoringWhitespace(state);
  if (IsEof(next)) {
    return nil;
  } else if (IsSymbolicQuote(next)) {
    return ReadQuote(state);
  } else if (IsStringQuote(next)) {
    return ReadString(state);
  } else if (IsHash(next)) {
    return ReadHash(state);
  } else if (IsNumberSign(next)) {
    return ReadSignedNumberOrSymbol(state);
  } else if (IsDigit(next)) {
    return ReadNumberOrSymbol(state);
  } else if (IsDecimalPoint(next)) {
    return ReadFloatingPointOrSymbol(state);
  } else if (IsOpenList(next)) {
    return ReadListLike(state);
  } else {
    return ReadSymbol(state);
  }
  ++state->sub_forms;
}

int ReadCharIgnoringWhitespace(struct ParseState *state) {
  int next = ReadChar(state);
  while (IsWhitespace(next))
    next = ReadChar(state);
  return next;
}

Object ReadSignedNumberOrSymbol(struct ParseState *state) {
  AppendReadBuffer(state->last_char);
  int next = ReadChar(state);
  if (IsDigit(next)) {
    return ReadNumberOrSymbol(state);
  }
  return ReadSymbol(state);
}

Object ReadNumberOrSymbol(struct ParseState *state) {
  AppendReadBuffer(state->last_char);
  while (1) {
    int next = ReadChar(state);
    if (IsWhitespace(next) || IsEof(next)) {
      Object string = FinalizeReadBuffer();
      s64 value = 0;
      assert(!IsEof(sscanf(StringCharacterBuffer(string), "%lld", &value)));
      return BoxFixnum(value);
    } else if (IsDigit(next)) {
      AppendReadBuffer(next);
    } else if (IsDecimalPoint(next) || IsExponentMarker(next)) {
      return ReadFloatingPointOrSymbol(state);
    } else {
      return ReadSymbol(state);
    }
  }
}

Object ReadFloatingPointOrSymbol(struct ParseState *state) {
  if (IsDecimalPoint(state->last_char)) {
    AppendReadBuffer(state->last_char);

    // Read 0 or more digits
    // followed by an optional exponent.
    while (1) {
      int next = ReadChar(state);
      if (IsWhitespace(next) || IsEof(next)) {
        if (ReadBufferLength() == 1) {
          // read buffer: .
          return InternSymbol(FinalizeReadBuffer());
        } else {
          Object string = FinalizeReadBuffer();
          // read buffer: {digit}+.{digit}* or {digit}*.{digit}+
          real64 value;
          assert(!IsEof(sscanf(StringCharacterBuffer(string), "%lf", &value)));
          return BoxReal64(value);
        }
      } else if (IsDigit(next)) {
        // read buffer: {digit}*.{digit}+
        AppendReadBuffer(next);
      } else if (IsExponentMarker(next)) {
        // read buffer: {digit}*.{digit}*e
        if (ReadBufferLength() == 1) {
          // read buffer: .e
          return ReadSymbol(state);
        } else {
          return ReadExponentOrSymbol(state);
        }
      } else {
        // read buffer: {digit}*.{digit}*x
        return ReadSymbol(state);
      }
    }
  } else if (IsExponentMarker(state->last_char)) {
    // read buffer: {digit}*e
    return ReadExponentOrSymbol(state);
  } else {
    printf("Invalid start to ReadFloatingPointOrSymbol: %c, while reading %s\n",
        state->last_char, StringCharacterBuffer(FinalizeReadBuffer()));
    PrintParseState(state);
    assert(!"error");
  }
}

Object ReadExponentOrSymbol(struct ParseState *state) {
  // exponent := exponent-marker [sign] {digit+}
  // read buffer: {decimal} {exponent-marker}

  int exponent_marker = state->last_char;
  AppendReadBuffer('e');

  int next = ReadChar(state);
  AppendReadBuffer(next);
  if (IsNumberSign(next)) {
    // read buffer: {decimal} {exponent-marker} {sign}
    // Read 1 or more digits.

    next = ReadChar(state);
    if (IsDigit(next)) {
      AppendReadBuffer(next);
      // read buffer: {decimal} {exponent-marker} {sign} {digit}

      // Read 0+ digits
      while (1) {
        next = ReadChar(state);
        if (IsDigit(next)) {
          // read buffer: {decimal} {exponent-marker} {sign} {digit}+
          AppendReadBuffer(next);
        } else if (IsWhitespace(next) || IsEof(next)) {
          // read buffer: {decimal} {exponent-marker} {sign} {digit}+
          if (exponent_marker == 's' || exponent_marker == 'S') {
            real32 value;
            assert(!IsEof(sscanf(StringCharacterBuffer(FinalizeReadBuffer()), "%f", &value)));
            return BoxReal32(value);
          } else {
            real64 value;
            assert(!IsEof(sscanf(StringCharacterBuffer(FinalizeReadBuffer()), "%lf", &value)));
            return BoxReal64(value);
          }
        } else {
          // read buffer: {decimal} e {exponent-marker} {sign} {digit}+ {other}
          return ReadSymbol(state);
        }
      }
    } else if (IsWhitespace(next) || IsEof(next)) {
      // read buffer: {decimal} e
      printf("Invalid symbol or floating point: %s. Expected exponent after exponent marker & sign.\n",
          StringCharacterBuffer(FinalizeReadBuffer()));
      PrintParseState(state);
      assert(!"error");
    } else {
      // read buffer: {decimal} {exponent-marker} {sign}
      return ReadSymbol(state);
    }
  } else if (IsDigit(next)) {
    // read buffer: {decimal} {exponent-marker} {digit}
    // Read 0 or more digits.
    while (1) {
      next = ReadChar(state);
      if (IsDigit(next)) {
        // read buffer: {decimal} e {exponent-marker} {digit}+
        AppendReadBuffer(next);
      } else if (IsWhitespace(next) || IsEof(next)) {
        // read buffer: {decimal} e {exponent-marker} {digit}+
        // TODO consolidate
        if (exponent_marker == 's' || exponent_marker == 'S') {
          real32 value;
          assert(!IsEof(sscanf(StringCharacterBuffer(FinalizeReadBuffer()), "%f", &value)));
          return BoxReal32(value);
        } else {
          real64 value;
          assert(!IsEof(sscanf(StringCharacterBuffer(FinalizeReadBuffer()), "%lf", &value)));
          return BoxReal64(value);
        }
      } else {
        // read buffer: {decimal} e {exponent-marker} {digit}+ {other}
        return ReadSymbol(state);
      }
    }
  } else if (IsWhitespace(next) || IsEof(next)) {
    // read buffer: {decimal} {exponent-marker}
    printf("Invalid symbol or floating point: %s. Expected exponent after exponent marker.\n",
        StringCharacterBuffer(FinalizeReadBuffer()));
    PrintParseState(state);
    assert(!"error");
  } else {
    return ReadSymbol(state);
  }
}

Object ReadQuote(struct ParseState *state) {
  Object object = ReadObject(state);
  SetRegister(REGISTER_READ_QUOTED_OBJECT, object);
  Object symbol = InternSymbol(AllocateString("quote"));
  // REFERENCES INVALIDATED

  Object quoted_object = MakePair(symbol, MakePair(GetRegister(REGISTER_READ_QUOTED_OBJECT), nil));
  return quoted_object;
}
Object ReadListLike(struct ParseState *state) { return nil; }

int HandleEscape(struct ParseState *state, int next) {
  if (IsEscape(next)) {
    next = ReadChar(state);
    if (IsEof(next)) {
      PrintParseState(state);
      assert(!"Error escaping EOF");
    }
  }
  return next;
}

Object ReadString(struct ParseState *state) {
  for (int next = ReadChar(state);
      !(IsStringQuote(next) || IsEof(next));
      next = ReadChar(state)) {
    AppendReadBuffer(HandleEscape(state, next));
  }
  return FinalizeReadBuffer();
}

Object ReadSymbol(struct ParseState *state) {
  for (int next = state->last_char;
      !(IsCloseList(next) || IsWhitespace(next) || IsEof(next));
      next = ReadChar(state)) {
    AppendReadBuffer(HandleEscape(state, next));
  }
  Object name = FinalizeReadBuffer();
  return InternSymbol(name);
}

Object ReadHash(struct ParseState *state) {
  ReadChar(state); // Discard the # when reading the symbol
  Object symbol = ReadSymbol(state);
  u8 *string = StringCharacterBuffer(symbol);

  if (!strcmp(string, "f")) return false;
  if (!strcmp(string, "t")) return true;

  printf("Invalid hash #%s\n", string);
  PrintParseState(state);
  assert(!"Invalid hash");
}

void PrintParseState(struct ParseState *state) {
  printf("Parse State: last_char=%c, eof?=%s, char_position=%llu, line_number=%llu, form_number=%llu\n",
      state->last_char, state->eof ? "true" : "false", state->chars, state->lines, state->sub_forms);
}

void TestParse() {
  InitializeMemory(128);
  InitializeSymbolTable(13);
  // symbol
  struct ParseState state = MakeParseState(MakeStringStream("   symbol  "));
  Object object = ReadObject(&state);
  assert(IsSymbol(object));
  assert(!strcmp(StringCharacterBuffer(object), "symbol"));

  state = MakeParseState(MakeStringStream("   \"Hello \\\"world\\\"\"  "));
  object = ReadObject(&state);
  assert(IsString(object));
  assert(!strcmp(StringCharacterBuffer(object), "Hello \"world\""));

  state = MakeParseState(MakeStringStream("  #f "));
  object = ReadObject(&state);
  assert(IsFalse(object));

  state = MakeParseState(MakeStringStream("  #t "));
  object = ReadObject(&state);
  assert(IsTrue(object));

  state = MakeParseState(MakeStringStream(" 'symbol "));
  object = ReadObject(&state);
  assert(IsPair(object));
  assert(!strcmp(StringCharacterBuffer(Car(object)), "quote"));
  assert(!strcmp(StringCharacterBuffer(Car(Cdr(object))), "symbol"));

  state = MakeParseState(MakeStringStream(" + "));
  object = ReadObject(&state);
  assert(IsSymbol(object));
  assert(!strcmp(StringCharacterBuffer(object), "+"));

  state = MakeParseState(MakeStringStream(" +157 "));
  object = ReadObject(&state);
  assert(IsFixnum(object));
  assert(UnboxFixnum(object) == 157);

  state = MakeParseState(MakeStringStream(" -1234567"));
  object = ReadObject(&state);
  assert(IsFixnum(object));
  assert(UnboxFixnum(object) == -1234567);

  state = MakeParseState(MakeStringStream(" 3.14159 "));
  object = ReadObject(&state);
  assert(IsReal64(object));
  assert(UnboxReal64(object) == 3.14159);

  state = MakeParseState(MakeStringStream(" 3.14159x "));
  object = ReadObject(&state);
  assert(IsSymbol(object));
  assert(!strcmp(StringCharacterBuffer(object), "3.14159x"));

  state = MakeParseState(MakeStringStream(" .123 "));
  object = ReadObject(&state);
  assert(IsReal64(object));
  assert(UnboxReal64(object) == .123);

  state = MakeParseState(MakeStringStream(" 1. "));
  object = ReadObject(&state);
  assert(IsReal64(object));
  assert(UnboxReal64(object) == 1.0);

  state = MakeParseState(MakeStringStream(" . "));
  object = ReadObject(&state);
  assert(IsSymbol(object));
  assert(!strcmp(StringCharacterBuffer(object), "."));

  state = MakeParseState(MakeStringStream(" .1e4 "));
  object = ReadObject(&state);
  assert(IsReal64(object));
  assert(UnboxReal64(object) == 1000.0);

  state = MakeParseState(MakeStringStream(" .1g "));
  object = ReadObject(&state);
  assert(IsSymbol(object));
  assert(!strcmp(StringCharacterBuffer(object), ".1g"));

  state = MakeParseState(MakeStringStream(" .g "));
  object = ReadObject(&state);
  assert(IsSymbol(object));
  assert(!strcmp(StringCharacterBuffer(object), ".g"));

  state = MakeParseState(MakeStringStream(" .e-g "));
  object = ReadObject(&state);
  assert(IsSymbol(object));
  assert(!strcmp(StringCharacterBuffer(object), ".e-g"));

  state = MakeParseState(MakeStringStream(" .e-1 "));
  object = ReadObject(&state);
  assert(IsSymbol(object));
  assert(!strcmp(StringCharacterBuffer(object), ".e-1"));

  state = MakeParseState(MakeStringStream(" 1e3 "));
  object = ReadObject(&state);
  assert(IsReal64(object));
  assert(UnboxReal64(object) == 1000.0);

  state = MakeParseState(MakeStringStream(" 1.e3 "));
  object = ReadObject(&state);
  assert(IsReal64(object));
  assert(UnboxReal64(object) == 1000.0);

  state = MakeParseState(MakeStringStream(" 1.0e3 "));
  object = ReadObject(&state);
  assert(IsReal64(object));
  assert(UnboxReal64(object) == 1000.0);

  state = MakeParseState(MakeStringStream(" 1.0E3 "));
  object = ReadObject(&state);
  assert(IsReal64(object));
  assert(UnboxReal64(object) == 1000.0);

  state = MakeParseState(MakeStringStream(" 1.0d3 "));
  object = ReadObject(&state);
  assert(IsReal64(object));
  assert(UnboxReal64(object) == 1000.0);

  state = MakeParseState(MakeStringStream(" 1.0s3 "));
  object = ReadObject(&state);
  assert(IsReal32(object));
  assert(UnboxReal32(object) == 1000.0f);
}

int main(int argc, char** argv) {
  TestTag();
  TestMemory();
  TestSymbolTable();
  TestReadBuffer();
  TestParse();
}

// TODO: handle closing paren when reading symbols, numbers, etc.
// TODO: establish calling convention for ReadBlah
