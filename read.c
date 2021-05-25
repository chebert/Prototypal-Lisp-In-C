#include "read.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Reads the next Object from source, leaving the resulting object in REGISTER_READ_OBJECT.
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
  return GetRegister(REGISTER_READ_RESULT);
}
void SetReadResult(Object object) {
  SetRegister(REGISTER_READ_RESULT, object);
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
    printf("Error: Unterminated string inside of list\n");
    *success = 0;
  } else if (token->type == TOKEN_EOF) {
    printf("Error: Unterminated list\n");
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
    printf("Error: Invalid placement for pair separator\n");
    *success = 0;
  } else if (token->type == TOKEN_LIST_CLOSE) {
    printf("Error: Unmatched list close\n");
    *success = 0;
  }
  return source;
}

const u8 *ReadNextListObject(const struct Token *token, const u8 *source, b64 *success) {
  // (a b c d)
  //    ^-here
  source = ReadNextObjectFromToken(token, source, success);
  printf("ReadNextListObject: Read in ");
  PrintlnObject(GetReadResult());
  if (!*success) return source;
  // (a b c d)
  //     ^-here
  // stack: ((a . nil))
  PushReadStack(MakePair(GetReadResult(), nil));
  // REFERENCES INVALIDATED
  printf("ReadNextListObject: pushing it onto the read stack: ");
  PrintlnObject(GetRegister(REGISTER_READ_STACK));
  
  // stack: ((b . nil) (a . nil))
  source = ContinueReadList(source, success);
  if (!*success) return source;
  // (a b c d)
  //          ^-here
  printf("ReadNextListObject: Read the rest of the list: ");
  PrintlnObject(GetReadResult());

  // stack: ((b . nil) (a . nil))
  // read result: (c . (d . nil))
  Object pair = PopReadStack();
  printf("ReadNextListObject: popping the read stack: ");
  PrintlnObject(GetRegister(REGISTER_READ_STACK));

  // stack: ((a . nil))
  SetCdr(pair, GetReadResult());
  SetReadResult(pair);
  printf("ReadNextListObject: setting the read result: ");
  PrintlnObject(GetReadResult());
  // stack: ((a . nil))
  // read result: (b . (c . nil))
  return source;
}

const u8 *BeginReadList(const u8 *source, b64 *success) {
  struct Token token;
  NextToken(source, &token);
  if (token.type == TOKEN_PAIR_SEPARATOR) {
    printf("Error: Attempt to parse pair separator in the first position of list\n");
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
      printf("Error: expected close-list after object following pair separator\n");
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
    assert(IsSymbol(GetReadResult()));
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
    assert(IsSymbol(Car(GetReadResult())));
    assert(IsSymbol(Cdr(GetReadResult())));
  }

  // Read List
  {
    const u8* source = " (a)";
    source = ReadObject(source, &success);
    assert(success);
    assert(IsPair(GetReadResult()));
    assert(IsSymbol(Car(GetReadResult())));
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

    assert(IsSymbol(Car(t)));
    assert(IsSymbol(Cdr(t)));
    assert(IsSymbol(Car(v)));
    assert(IsSymbol(Cdr(v)));
    assert(IsSymbol(Car(w)));
    assert(IsSymbol(Cdr(w)));
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

    assert(IsSymbol(Car(s))); // a
    assert(IsSymbol(Car(t))); // b
    assert(IsSymbol(Car(v))); // c
    assert(IsSymbol(Car(w))); // d
    assert(IsSymbol(Cdr(w))); // e
    assert(IsSymbol(Car(y))); // f
    assert(IsSymbol(Car(z))); // g
    assert(IsNil(Cdr(z)));
    assert(IsSymbol(Car(S))); // h
    assert(IsNil(Cdr(S)));
  }

  DestroyMemory();
}

int main(int argc, char **argv) {
  TestTag();
  TestToken();
  TestMemory();
  TestSymbolTable();
  TestRead();
  return 0;
}
