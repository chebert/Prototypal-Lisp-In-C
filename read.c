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

// Pops the top of the Read stack
Object PopReadStack();
// Pushes the (read_result . nil) onto the top of the read stack. TODO: can we just push read_result?
void PushReadResultOntoStack(enum ErrorCode *error);
// Reads the next Object from token, and then source.
const u8 *ReadNextObjectFromToken(const struct Token *token, const u8 *source, enum ErrorCode *error);

// True if the token is a single token (i.e. not a list token or an error)
b64 IsSingleToken(const struct Token *token);
// Construct & return Object given a single token.
Object ProcessSingleToken(const struct Token *token, enum ErrorCode *error);

// True if the token is an error (like EOF or an unterminated string).
b64 IsErrorToken(const struct Token *token);
// Prints an error message and sets error.
void HandleErrorToken(const struct Token *token, enum ErrorCode *error);

// Reads the next Object from source, leaving the resulting object in REGISTER_EXPRESSION.
// Returns the remaining unconsumed source.
const u8 *ReadNextObject(const u8 *source, enum ErrorCode *error);
// Function to begin reading a list.
const u8 *BeginReadList(const u8 *source, enum ErrorCode *error);
// Functions to continue reading a list.
const u8 *ContinueReadList(const u8 *source, enum ErrorCode *error);
const u8 *ReadNextListObject(const struct Token *token, const u8 *source, enum ErrorCode *error);

const u8 *ReadObject(const u8 *source, enum ErrorCode *error) {
  return ReadNextObject(source, error);
}

Object PopReadStack() {
  Object top = Car(GetRegister(REGISTER_READ_STACK));
  SetRegister(REGISTER_READ_STACK, Cdr(GetRegister(REGISTER_READ_STACK)));
  return top;
}

void PushReadResultOntoStack(enum ErrorCode *error) {
  Object read_stack = AllocatePair(error);
  if (*error) return;

  SetCdr(read_stack, GetRegister(REGISTER_READ_STACK));
  SetRegister(REGISTER_READ_STACK, read_stack);

  Object pair = AllocatePair(error);
  if (*error) return;

  SetCar(pair, GetRegister(REGISTER_EXPRESSION));
  SetCar(GetRegister(REGISTER_READ_STACK), pair);
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

void HandleErrorToken(const struct Token *token, enum ErrorCode *error) {
  if (token->type == TOKEN_UNTERMINATED_STRING) {
    LOG_ERROR("Unterminated string inside of list\n");
    *error = ERROR_READ_UNTERMINATED_STRING;
  } else if (token->type == TOKEN_EOF) {
    LOG_ERROR("Unterminated list\n");
    *error = ERROR_READ_UNTERMINATED_LIST;
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

Object ProcessSingleToken(const struct Token *token, enum ErrorCode *error) {
  u8 *data = CopyTokenSource(token);
  Object object = nil;
  if (token->type == TOKEN_INTEGER) {
    s64 value;
    if (!sscanf(data, "%lld", &value)) {
      *error = ERROR_READ_INVALID_INTEGER;
      object = nil;
    } else {
      object = BoxFixnum(value);
    }
  } else if (token->type == TOKEN_REAL32) {
    real32 value;
    if (!sscanf(data, "%f", &value)) {
      *error = ERROR_READ_INVALID_REAL32;
      object = nil;
    } else  {
      object = BoxReal32(value);
    }
  } else if (token->type == TOKEN_REAL64) {
    real64 value;
    if (!sscanf(data, "%lf", &value)) {
      *error = ERROR_READ_INVALID_REAL64;
      object = nil;
    } else {
      object = BoxReal64(value);
    }
  } else if (token->type == TOKEN_STRING) {
    object = AllocateString(data, error);
    // REFERENCES INVALIDATED
  } else if (token->type == TOKEN_SYMBOL) {
    object = InternSymbol(data, error);
    // REFERENCES INVALIDATED
  }
  free(data);
  return object;
}

const u8 *ReadNextObject(const u8 *source, enum ErrorCode *error) {
  struct Token token;
  source = NextToken(source, &token);
  return ReadNextObjectFromToken(&token, source, error);
}

const u8 *ReadNextObjectFromToken(const struct Token *token, const u8 *source, enum ErrorCode *error) {
  if (IsSingleToken(token)) {
    SetRegister(REGISTER_EXPRESSION, ProcessSingleToken(token, error));
  } else if (token->type == TOKEN_LIST_OPEN) {
    source = BeginReadList(source, error);
  } else if (token->type == TOKEN_SYMBOLIC_QUOTE) {
    source = ReadNextObject(source, error);
    if (*error) return source;

    // read-result: object
    Object result = AllocatePair(error);
    if (*error) return source;
    SetCar(result, GetRegister(REGISTER_EXPRESSION));
    SetCdr(result, nil);
    SetRegister(REGISTER_EXPRESSION, result);
    // read-result: (object)

    result = AllocatePair(error);
    if (*error) return source;
    Object quote = FindSymbol("quote");
    assert(!IsNil(quote));
    SetCar(result, FindSymbol("quote"));
    SetCdr(result, GetRegister(REGISTER_EXPRESSION));
    SetRegister(REGISTER_EXPRESSION, result);
    // read-result: (quote object)
  } else if (token->type == TOKEN_LINE_COMMENT) {
    // discard
    source = ReadNextObject(source, error);
  } else if (IsErrorToken(token)) {
    HandleErrorToken(token, error);
  } else if (token->type == TOKEN_PAIR_SEPARATOR) {
    LOG_ERROR("Invalid placement for pair separator\n");
    *error = ERROR_READ_INVALID_PAIR_SEPARATOR;
  } else if (token->type == TOKEN_LIST_CLOSE) {
    LOG_ERROR("Unmatched list close\n");
    *error = ERROR_READ_UNMATCHED_LIST_CLOSE;
  }
  return source;
}

const u8 *ReadNextListObject(const struct Token *token, const u8 *source, enum ErrorCode *error) {
  // (a b c d)
  //    ^-here
  source = ReadNextObjectFromToken(token, source, error);
  LOG(LOG_READ, "Read in ");
  LOG_OP(LOG_READ, PrintlnObject(GetRegister(REGISTER_EXPRESSION)));
  if (*error) return source;
  // (a b c d)
  //     ^-here
  // stack: (a)
  PushReadResultOntoStack(error);
  if (*error) return source;

  LOG(LOG_READ, "pushing it onto the read stack: ");
  LOG_OP(LOG_READ, PrintlnObject(GetRegister(REGISTER_READ_STACK)));
  
  // stack: ((b . nil) (a . nil))
  source = ContinueReadList(source, error);
  if (*error) return source;
  // (a b c d)
  //          ^-here
  LOG(LOG_READ, "Read the rest of the list: ");
  LOG_OP(LOG_READ, PrintlnObject(GetRegister(REGISTER_EXPRESSION)));

  // stack: ((b . nil) (a . nil))
  // read result: (c . (d . nil))
  Object pair = PopReadStack();
  LOG(LOG_READ, "popping the read stack: ");
  LOG_OP(LOG_READ, PrintlnObject(GetRegister(REGISTER_READ_STACK)));

  // stack: ((a . nil))
  SetCdr(pair, GetRegister(REGISTER_EXPRESSION));
  SetRegister(REGISTER_EXPRESSION, pair);
  LOG(LOG_READ, "setting the read result: ");
  LOG_OP(LOG_READ, PrintlnObject(GetRegister(REGISTER_EXPRESSION)));
  // stack: ((a . nil))
  // read result: (b . (c . nil))
  return source;
}

const u8 *BeginReadList(const u8 *source, enum ErrorCode *error) {
  struct Token token;
  NextToken(source, &token);
  if (token.type == TOKEN_PAIR_SEPARATOR) {
    LOG_ERROR("Attempt to parse pair separator in the first position of list\n");
    *error = ERROR_READ_PAIR_SEPARATOR_IN_FIRST_POSITION;
    return source;
  }
  return ContinueReadList(source, error);
}

const u8 *ContinueReadList(const u8 *source, enum ErrorCode *error) {
  // ( a b c ... )
  //    ^-here    
  struct Token token;
  source = NextToken(source, &token);
  *error = NO_ERROR;

  // List is the form (a b c)
  //                        ^-here
  if (token.type == TOKEN_LIST_CLOSE) {
    // read result: ()
    SetRegister(REGISTER_EXPRESSION, nil);

  // List ends with a dotted pair
  } else if (token.type == TOKEN_PAIR_SEPARATOR) {
    // (... c . d)
    //         ^-here
    source = ReadNextObject(source, error);
    if (*error) return source;
    // (... c . d)
    //           ^-here
    source = NextToken(source, &token);
    // (... c . d)
    //            ^-here
    if (token.type != TOKEN_LIST_CLOSE) {
      LOG_ERROR("expected close-list after object following pair separator\n");
      *error = ERROR_READ_TOO_MANY_OBJECTS_IN_PAIR;
      return source;
    }
    // read result: d
    
  // List is malformed
  } else if (IsErrorToken(&token)) {
    HandleErrorToken(&token, error);

  // More elements in the list.
  } else {
    source = ReadNextListObject(&token, source, error);
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
  enum ErrorCode error = NO_ERROR;
  InternSymbol("quote", &error);

  // Read string
  {
    const u8* source = "\"abra\"";
    source = ReadObject(source, &error);
    assert(!error);
    assert(IsString(GetRegister(REGISTER_EXPRESSION)));
    assert(!strcmp("abra", StringCharacterBuffer(GetRegister(REGISTER_EXPRESSION))));
  }

  // Read integer
  {
    const u8* source = "-12345";
    source = ReadObject(source, &error);
    assert(!error);
    assert(IsFixnum(GetRegister(REGISTER_EXPRESSION)));
    assert(-12345 == UnboxFixnum(GetRegister(REGISTER_EXPRESSION)));
  }

  // Read real64
  {
    const u8* source = "-123.4e5";
    source = ReadObject(source, &error);
    assert(!error);
    assert(IsReal64(GetRegister(REGISTER_EXPRESSION)));
    assert(-123.4e5 == UnboxReal64(GetRegister(REGISTER_EXPRESSION)));
  }

  // Read symbol
  {
    const u8* source = "the-symbol";
    source = ReadObject(source, &error);
    assert(!error);
    assert(SymbolEq(GetRegister(REGISTER_EXPRESSION), "the-symbol"));
    assert(!strcmp("the-symbol", StringCharacterBuffer(GetRegister(REGISTER_EXPRESSION))));
  }

  // Read ()
  {
    const u8* source = " (     \n)";
    source = ReadObject(source, &error);
    assert(!error);
    assert(IsNil(GetRegister(REGISTER_EXPRESSION)));
  }

  // Read Pair
  {
    const u8* source = " (a . b)";
    source = ReadObject(source, &error);
    assert(!error);
    assert(IsPair(GetRegister(REGISTER_EXPRESSION)));
    assert(SymbolEq(Car(GetRegister(REGISTER_EXPRESSION)), "a"));
    assert(SymbolEq(Cdr(GetRegister(REGISTER_EXPRESSION)), "b"));
  }

  // Read List
  {
    const u8* source = " (a)";
    source = ReadObject(source, &error);
    assert(!error);
    assert(IsPair(GetRegister(REGISTER_EXPRESSION)));
    assert(SymbolEq(Car(GetRegister(REGISTER_EXPRESSION)), "a"));
    assert(IsNil(Cdr(GetRegister(REGISTER_EXPRESSION))));
  }

  // Read Dotted list: nested
  {
    const u8* source = " ((a . b) (c . d) . (e . f))";
    //                    st      uv         w
    source = ReadObject(source, &error);
    assert(!error);
    Object s = GetRegister(REGISTER_EXPRESSION);
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
    source = ReadObject(source, &error);
    assert(!error);
    Object s = GetRegister(REGISTER_EXPRESSION);
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
    source = ReadObject(source, &error);
    assert(!error);
    Object s = GetRegister(REGISTER_EXPRESSION);
    Object t = Cdr(s);
    Object u = Car(t);
    Object v = Cdr(u);

    assert(SymbolEq(Car(s), "quote"));
    assert(SymbolEq(Car(u), "a"));
    assert(SymbolEq(Car(v), "b"));
  }

  // TODO: read some invalid forms

  DestroyMemory();
}
