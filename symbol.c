#include "symbol.h"

#include <stdio.h>

#include "blob.h"
#include "string.h"

Object MoveSymbol(struct Memory *memory, u64 ref) { 
  return BoxSymbol(MoveBlob(memory, ref));
}

Object AllocateSymbol(struct Memory *memory, const char *name) {
  return BoxSymbol(AllocateString(memory, name));
}

void PrintSymbol(struct Memory *memory, Object symbol) {
  u64 reference = UnboxReference(symbol);
  // TODO: Print escaping characters
  printf("%s", (const char*)&memory->the_objects[reference+1]);
}
