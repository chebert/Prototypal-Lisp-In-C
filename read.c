#include "read.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "memory.h"
#include "pair.h"
#include "root.h"
#include "string.h"
#include "symbol_table.h"
#include "token.h"

// Returns the current memory-register read result
Object GetReadResult();
// Sets the current memory-register read result to object
void SetReadResult(Object object);

// Pops the top of the Read stack
Object PopReadStack();
// Pushes pair onto the top of the Read stack.
void PushReadStack(Object pair);
// Reads the next Object from token, and then source.
const u8 *ReadNextObjectFromToken(const struct Token *token, const u8 *source, b64 *success);

// True if the token is a single token (i.e. not a list token or an error)
b64 IsSingleToken(const struct Token *token);
// Construct & return Object given a single token.
Object ProcessSingleToken(const struct Token *token, b64 *success);

// True if the token is an error (like EOF or an unterminated string).
b64 IsErrorToken(const struct Token *token);
// Prints an error message and sets success to false.
void HandleErrorToken(const struct Token *token, b64 *success);

// Reads the next Object from source, leaving the resulting object in REGISTER_EXPRESSION.
// Returns the remaining unconsumed source.
// success is marked as true upon reading the full object.
const u8 *ReadNextObject(const u8 *source, b64 *success);
// Function to begin reading a list.
const u8 *BeginReadList(const u8 *source, b64 *success);
// Functions to continue reading a list.
const u8 *ContinueReadList(const u8 *source, b64 *success);
const u8 *ReadNextListObject(const struct Token *token, const u8 *source, b64 *success);

const u8 *ReadObject(const u8 *source, b64 *success) {
  SetRegister(REGISTER_READ_STACK, nil);
  source = ReadNextObject(source, success);
  SetRegister(REGISTER_READ_STACK, nil);
  return source;
}

Object GetReadResult() {
  return GetRegister(REGISTER_EXPRESSION);
}
void SetReadResult(Object object) {
  SetRegister(REGISTER_EXPRESSION, object);
}

Object PopReadStack() {
  Object top = Car(GetRegister(REGISTER_READ_STACK));
  SetRegister(REGISTER_READ_STACK, Cdr(GetRegister(REGISTER_READ_STACK)));
  return top;
}
void PushReadStack(Object pair) {
  SetRegister(REGISTER_READ_STACK,
      MakePair(pair, GetRegister(REGISTER_READ_STACK)));
}

b64 IsErrorToken(const struct Token *token) {
  switch (token->type) {
    case TOKEN_EOF:
    case TOKEN_UNTERMINATED_STRING:
      return 1;
    default:
      return 0;
  }
}

void HandleErrorToken(const struct Token *token, b64 *success) {
  if (token->type == TOKEN_UNTERMINATED_STRING) {
    LOG_ERROR("Unterminated string inside of list\n");
    *success = 0;
  } else if (token->type == TOKEN_EOF) {
    LOG_ERROR("Unterminated list\n");
    *success = 0;
  }
}

b64 IsSingleToken(const struct Token *token) {
  switch (token->type) {
    case TOKEN_INTEGER:
    case TOKEN_REAL32:
    case TOKEN_REAL64:
    case TOKEN_STRING:
    case TOKEN_SYMBOL:
      return 1;
    default:
      return 0;
  }
}

Object ProcessSingleToken(const struct Token *token, b64 *success) {
  u8 *data = CopyTokenSource(token);
  Object object = nil;
  if (token->type == TOKEN_INTEGER) {
    s64 value;
    sscanf(data, "%lld", &value);
    object = BoxFixnum(value);
  } else if (token->type == TOKEN_REAL32) {
    real32 value;
    sscanf(data, "%f", &value);
    object = BoxReal32(value);
  } else if (token->type == TOKEN_REAL64) {
    real64 value;
    sscanf(data, "%lf", &value);
    object = BoxReal64(value);
  } else if (token->type == TOKEN_STRING) {
    object = AllocateString(data);
    // REFERENCES INVALIDATED
  } else if (token->type == TOKEN_SYMBOL) {
    object = InternSymbol(data);
    // REFERENCES INVALIDATED
  }
  free(data);
  *success = 1;
  return object;
}

const u8 *ReadNextObject(const u8 *source, b64 *success) {
  struct Token token;
  source = NextToken(source, &token);
  return ReadNextObjectFromToken(&token, source, success);
}

const u8 *ReadNextObjectFromToken(const struct Token *token, const u8 *source, b64 *success) {
  if (IsSingleToken(token)) {
    SetReadResult(ProcessSingleToken(token, success));
  } else if (token->type == TOKEN_LIST_OPEN) {
    source = BeginReadList(source, success);
  } else if (token->type == TOKEN_SYMBOLIC_QUOTE) {
    source = ReadNextObject(source, success);
    if (!*success)
      return source;

    SetReadResult(MakePair(InternSymbol("quote"), MakePair(GetReadResult(), nil)));
    // REFERENCES INVALIDATED
  } else if (token->type == TOKEN_LINE_COMMENT) {
    // discard
    source = ReadNextObject(source, success);
  } else if (IsErrorToken(token)) {
    HandleErrorToken(token, success);
  } else if (token->type == TOKEN_PAIR_SEPARATOR) {
    LOG_ERROR("Invalid placement for pair separator\n");
    *success = 0;
  } else if (token->type == TOKEN_LIST_CLOSE) {
    LOG_ERROR("Unmatched list close\n");
    *success = 0;
  }
  return source;
}

const u8 *ReadNextListObject(const struct Token *token, const u8 *source, b64 *success) {
  // (a b c d)
  //    ^-here
  source = ReadNextObjectFromToken(token, source, success);
  LOG("Read in ");
  PrintlnObject(GetReadResult());
  if (!*success) return source;
  // (a b c d)
  //     ^-here
  // stack: ((a . nil))
  PushReadStack(MakePair(GetReadResult(), nil));
  // REFERENCES INVALIDATED
  LOG("pushing it onto the read stack: ");
  PrintlnObject(GetRegister(REGISTER_READ_STACK));
  
  // stack: ((b . nil) (a . nil))
  source = ContinueReadList(source, success);
  if (!*success) return source;
  // (a b c d)
  //          ^-here
  LOG("Read the rest of the list: ");
  PrintlnObject(GetReadResult());

  // stack: ((b . nil) (a . nil))
  // read result: (c . (d . nil))
  Object pair = PopReadStack();
  LOG("popping the read stack: ");
  PrintlnObject(GetRegister(REGISTER_READ_STACK));

  // stack: ((a . nil))
  SetCdr(pair, GetReadResult());
  SetReadResult(pair);
  LOG("setting the read result: ");
  PrintlnObject(GetReadResult());
  // stack: ((a . nil))
  // read result: (b . (c . nil))
  return source;
}

const u8 *BeginReadList(const u8 *source, b64 *success) {
  struct Token token;
  NextToken(source, &token);
  if (token.type == TOKEN_PAIR_SEPARATOR) {
    LOG_ERROR("Attempt to parse pair separator in the first position of list\n");
    *success = 0;
  }
  return ContinueReadList(source, success);
}

const u8 *ContinueReadList(const u8 *source, b64 *success) {
  // ( a b c ... )
  //    ^-here    
  struct Token token;
  source = NextToken(source, &token);
  *success = 1;

  // List is the form (a b c)
  //                        ^-here
  if (token.type == TOKEN_LIST_CLOSE) {
    // read result: ()
    SetReadResult(nil);

  // List ends with a dotted pair
  } else if (token.type == TOKEN_PAIR_SEPARATOR) {
    // (... c . d)
    //         ^-here
    source = ReadNextObject(source, success);
    if (!*success) return source;
    // (... c . d)
    //           ^-here
    source = NextToken(source, &token);
    // (... c . d)
    //            ^-here
    if (token.type != TOKEN_LIST_CLOSE) {
      LOG_ERROR("expected close-list after object following pair separator\n");
      *success = 0;
      return source;
    }
    // read result: d
    
  // List is malformed
  } else if (IsErrorToken(&token)) {
    HandleErrorToken(&token, success);

  // More elements in the list.
  } else {
    source = ReadNextListObject(&token, source, success);
    // read result (b . (c . nil))
  }
  return source;
}

b64 SymbolEq(Object symbol, const u8 *name) {
  return IsSymbol(symbol) && !strcmp(StringCharacterBuffer(symbol), name);
}

void TestRead() {
  InitializeMemory(128);
  InitializeSymbolTable(1);
  b64 success = 0;

  // Read string
  {
    const u8* source = "\"abra\"";
    source = ReadObject(source, &success);
    assert(success);
    assert(IsString(GetReadResult()));
    assert(!strcmp("abra", StringCharacterBuffer(GetReadResult())));
  }

  // Read integer
  {
    const u8* source = "-12345";
    source = ReadObject(source, &success);
    assert(success);
    assert(IsFixnum(GetReadResult()));
    assert(-12345 == UnboxFixnum(GetReadResult()));
  }

  // Read real64
  {
    const u8* source = "-123.4e5";
    source = ReadObject(source, &success);
    assert(success);
    assert(IsReal64(GetReadResult()));
    assert(-123.4e5 == UnboxReal64(GetReadResult()));
  }

  // Read symbol
  {
    const u8* source = "the-symbol";
    source = ReadObject(source, &success);
    assert(success);
    assert(SymbolEq(GetReadResult(), "the-symbol"));
    assert(!strcmp("the-symbol", StringCharacterBuffer(GetReadResult())));
  }

  // Read ()
  {
    const u8* source = " (     \n)";
    source = ReadObject(source, &success);
    assert(success);
    assert(IsNil(GetReadResult()));
  }

  // Read Pair
  {
    const u8* source = " (a . b)";
    source = ReadObject(source, &success);
    assert(success);
    assert(IsPair(GetReadResult()));
    assert(SymbolEq(Car(GetReadResult()), "a"));
    assert(SymbolEq(Cdr(GetReadResult()), "b"));
  }

  // Read List
  {
    const u8* source = " (a)";
    source = ReadObject(source, &success);
    assert(success);
    assert(IsPair(GetReadResult()));
    assert(SymbolEq(Car(GetReadResult()), "a"));
    assert(IsNil(Cdr(GetReadResult())));
  }

  // Read Dotted list: nested
  {
    const u8* source = " ((a . b) (c . d) . (e . f))";
    //                    st      uv         w
    source = ReadObject(source, &success);
    assert(success);
    Object s = GetReadResult(s);
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
    source = ReadObject(source, &success);
    assert(success);
    Object s = GetReadResult();
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
    source = ReadObject(source, &success);
    assert(success);
    Object s = GetReadResult();
    Object t = Cdr(s);
    Object u = Car(t);
    Object v = Cdr(u);

    assert(SymbolEq(Car(s), "quote"));
    assert(SymbolEq(Car(u), "a"));
    assert(SymbolEq(Car(v), "b"));
  }

  DestroyMemory();
}
