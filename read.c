#include "token.h"

// Returns the current memory-register read result
Object ReadResult();
// Sets the current memory-register read result to object
void SetReadResult(Object object);

// Pops the top of the Read stack
Object PopReadStack();
// Pushes pair onto the top of the Read stack.
void PushReadStack(Object pair);

// Reads the next Object from source, leaving the resulting object in REGISTER_READ_OBJECT.
// Returns the remaining unconsumed source.
// success is marked as true upon reading the full object.
const u8 *ReadNextObject(const u8 *source, b64 *success);

// True if the token is a primitive object.
b64 IsPrimitiveToken(struct Token *token);
// Construct & return Object given a primitive token.
Object ProcessPrimitiveToken(struct Token *token, b64 *success);

// True if the token is an error (like EOF or an unterminated string).
b64 IsErrorToken(struct Token *token);
// Prints an error message and sets success to false.
void HandleErrorToken(struct Token *token, b64 *success);

// Function to begin reading a list.
const u8 *BeginReadList(const u8 *source, b64 *success);
// Functions to continue reading a list.
const u8 *ContinueReadList(const u8 *source, b64 *success);
const u8 *ReadNextListObject(const u8 *source, b64 *success);

Object ReadResult() {
  return GetRegister(REGISTER_READ_RESULT);
}
void SetReadResult(Object object) {
  SetRegister(REGISTER_READ_RESULT, object);
}

Object PopReadStack() {
  return Car(GetRegister(REGISTER_READ_STACK));
}
void PushReadStack(Object pair) {
  SetRegister(REGISTER_READ_STACK,
      MakePair(pair, GetRegister(REGISTER_READ_STACK)));
}

b64 IsErrorToken(struct Token *token) {
  switch (token->type) {
    case TOKEN_EOF:
    case TOKEN_UNTERMINATED_STRING:
      return 1;
    default:
      return 0;
  }
}

void HandleErrorToken(struct Token *token, b64 *success) {
  if (token->type == TOKEN_UNTERMINATED_STRING) {
    printf("Error: Unterminated string inside of list\n");
    *success = 0;
  } else if (token->type == TOKEN_LINE_EOF) {
    printf("Error: Unterminated list\n");
    *success = 0;
  }
}

b64 IsPrimitiveToken(struct Token *token) {
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

Object ProcessPrimitiveToken(struct Token *token, b64 *success) {
  u8 *data = CopyTokenSource(&token);
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
  } else if (token->type == TOKEN_SYMBOL) {
    object = Intern(AllocateString(data));
  }
  free(data);
  return object;
}

const u8 *ReadNextObject(const u8 *source, b64 *success) {
  struct Token token;
  source = NextToken(source, &token);
  *success = 1;

  if (IsPrimitiveToken(&token)) {
    SetReadResult(ProcessPrimitiveToken(&token));
  } else if (token.type == TOKEN_LIST_OPEN) {
    source = BeginReadList(source, success);
  } else if (token.type == TOKEN_SYMBOLIC_QUOTE) {
    source = ReadNextObject(source, success);
    if (!*success)
      return source;

    SetReadResult(MakePair(Intern(AllocateString("quote")), 
          MakePair(GetReadResult(), nil)));
  } else if (token.type == TOKEN_LINE_COMMENT) {
    // discard
    source = ReadNextObject(source, success);
  } else if (IsErrorToken(&token)) {
    HandleErrorToken(&token, success);
  } else if (token.type == TOKEN_PAIR_SEPARATOR) {
    printf("Error: Invalid placement for pair separator\n");
    *success = 0;
  } else if (token.type == TOKEN_LIST_CLOSE) {
    printf("Error: Unmatched list close\n");
    *success = 0;
  }
  return source;
}

const u8 *ReadNextListObject(const u8 *source, b64 *success) {
  // (a b c d)
  //    ^-here
  source = ReadNextObject(source, success);
  if (!*success) return source;
  // (a b c d)
  //     ^-here
  // stack: ((a . nil))
  PushReadStack(MakePair(GetReadResult(), nil));
  // stack: ((b . nil) (a . nil))
  source = ContinueReadList(source, success);
  if (!*success) return source;
  // (a b c d)
  //          ^-here

  // stack: ((b . nil) (a . nil))
  // read result: (c . (d . nil))
  Object pair = PopReadStack();
  // stack: ((a . nil))
  SetCdr(pair, GetReadResult());
  SetReadResult(pair);
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
  const u8 *next_source = NextToken(source, &token);
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
    next_source = ReadNextObject(next_source, &token, success);
    if (!*success) return next_source;
    // (... c . d)
    //           ^-here
    next_source = NextToken(next_source, &token);
    // (... c . d)
    //            ^-here
    if (token.type != TOKEN_LIST_CLOSE) {
      printf("Error: expected close-list after object following pair separator\n");
      *success = 0;
    }
    // read result: d
    
  // List is malformed
  } else if (IsErrorToken(&token)) {
    HandleErrorToken(&token, success);

  // More elements in the list.
  } else {
    next_source = ReadNextListObject(source, success);
    // read result (b . (c . nil))
  }
  return next_source;
}
