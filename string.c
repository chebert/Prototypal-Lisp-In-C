#include "string.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "blob.h"
#include "memory.h"

Object AllocateString(const char *string) {
  u64 num_bytes = strlen(string) + 1;
  u64 new_reference = AllocateBlob(num_bytes);
  memcpy(&memory.the_objects[new_reference + 1], string, num_bytes);
  return BoxString(new_reference);
}

Object MoveString(u64 ref) { 
  return BoxString(MoveBlob(ref));
}

void PrintString(Object object) {
  u64 reference = UnboxReference(object);
  printf("\"%s\"", (const char*)&memory.the_objects[reference+1]);
}
