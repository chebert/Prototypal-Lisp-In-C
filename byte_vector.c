#include "byte_vector.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "blob.h"

Object AllocateByteVector(struct Memory *memory, u64 num_bytes) {
  u64 new_reference = AllocateBlob(memory, num_bytes);
  memset(&memory->the_objects[new_reference + 1], 0, num_bytes);
  return BoxByteVector(new_reference);
}

Object MoveByteVector(struct Memory *memory, u64 ref) {
  return BoxByteVector(MoveBlob(memory, ref));
}

Object ByteVectorRef(struct Memory *memory, u64 reference, u64 index) {
  assert(index < UnboxFixnum(memory->the_objects[reference]));
  u8 *bytes = (u8*)&memory->the_objects[reference+1];
  return BoxFixnum(bytes[index]);
}
void ByteVectorSet(struct Memory *memory, u64 reference, u64 index, u8 value) {
  assert(index < UnboxFixnum(memory->the_objects[reference]));
  u8 *bytes = (u8*)&memory->the_objects[reference+1];
  bytes[index] = value;
}

void PrintByteVector(struct Memory *memory, Object object) {
  u64 reference = UnboxReference(object);
  printf("(byte-vector");
  s64 length = UnboxFixnum(memory->the_objects[reference]);
  u8 *bytes = (u8*)&memory->the_objects[reference+1];
  for (s64 index = 0; index < length; ++index) {
    printf(" 0x%x", bytes[index]);
  }
  printf(")");
}
