#include "byte_vector.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "blob.h"
#include "memory.h"

Object AllocateByteVector(u64 num_bytes) {
  u64 new_reference = AllocateBlob(num_bytes);
  memset(&memory.the_objects[new_reference + 1], 0, num_bytes);
  return BoxByteVector(new_reference);
}

Object MoveByteVector(u64 ref) {
  return BoxByteVector(MoveBlob(ref));
}

Object ByteVectorRef(u64 reference, u64 index) {
  assert(index < UnboxFixnum(memory.the_objects[reference]));
  u8 *bytes = (u8*)&memory.the_objects[reference+1];
  return BoxFixnum(bytes[index]);
}
void ByteVectorSet(u64 reference, u64 index, u8 value) {
  assert(index < UnboxFixnum(memory.the_objects[reference]));
  u8 *bytes = (u8*)&memory.the_objects[reference+1];
  bytes[index] = value;
}

void PrintByteVector(Object object) {
  u64 reference = UnboxReference(object);
  printf("(byte-vector");
  s64 length = UnboxFixnum(memory.the_objects[reference]);
  u8 *bytes = (u8*)&memory.the_objects[reference+1];
  for (s64 index = 0; index < length; ++index) {
    printf(" 0x%x", bytes[index]);
  }
  printf(")");
}
