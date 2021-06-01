#include "symbol.h"

#include <stdio.h>

#include "blob.h"
#include "memory.h"
#include "string.h"

Object MoveSymbol(Object symbol) { 
  return BoxSymbol(MoveBlob(UnboxReference(symbol)));
}

Object AllocateSymbol(const char *name, enum ErrorCode *error) {
  Object symbol = AllocateString(name, error);
  if (IsNil(symbol)) return nil;
  return BoxSymbol(symbol);
}

void PrintSymbol(Object symbol) {
  u64 reference = UnboxReference(symbol);
  // TODO: Print escaping characters
  printf("%s", (const char*)&memory.the_objects[reference+1]);
}
