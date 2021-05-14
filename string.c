#include "string.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "blob.h"

Object AllocateString(struct Memory *memory, const char *string) {
  u64 num_bytes = strlen(string) + 1;
  u64 new_reference = AllocateBlob(memory, num_bytes);
  memcpy(&memory->the_objects[new_reference + 1], string, num_bytes);
  return BoxString(new_reference);
}

Object MoveString(struct Memory *memory, u64 ref) { 
  return BoxString(MoveBlob(memory, ref));
}

void PrintString(struct Memory *memory, Object object) {
  u64 reference = UnboxReference(object);
  printf("\"%s\"", (const char*)&memory->the_objects[reference+1]);
}
