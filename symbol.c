#include "symbol.h"

#include <stdio.h>

#include "blob.h"
#include "memory.h"
#include "string.h"

Object MoveSymbol(u64 ref) { 
  return BoxSymbol(MoveBlob(ref));
}

Object AllocateSymbol(const char *name) {
  return BoxSymbol(AllocateString(name));
}

void PrintSymbol(Object symbol) {
  u64 reference = UnboxReference(symbol);
  // TODO: Print escaping characters
  printf("%s", (const char*)&memory.the_objects[reference+1]);
}
