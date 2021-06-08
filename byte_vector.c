#include "byte_vector.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "blob.h"
#include "log.h"
#include "memory.h"

Object AllocateByteVector(u64 num_bytes, enum ErrorCode *error) {
  u64 new_reference = AllocateBlob(num_bytes, error);
  if (*error) return nil;

  memset(&memory.the_objects[new_reference + 1], 0, num_bytes);
  return BoxByteVector(new_reference);
}

Object MoveByteVector(Object byte_vector) {
  return BoxByteVector(MoveBlob(UnboxReference(byte_vector)));
}

s64 UnsafeByteVectorLength(Object byte_vector) {
  assert(IsByteVector(byte_vector));
  return UnboxFixnum(memory.the_objects[UnboxReference(byte_vector)]);
}

Object UnsafeByteVectorRef(Object byte_vector, u64 index) {
  assert(IsByteVector(byte_vector));
  assert(index < UnsafeByteVectorLength(byte_vector));
  u8 *bytes = (u8*)&memory.the_objects[UnboxReference(byte_vector)+1];
  return BoxFixnum(bytes[index]);
}

void UnsafeByteVectorSet(Object byte_vector, u64 index, u8 value) {
  assert(IsByteVector(byte_vector));
  assert(index < UnsafeByteVectorLength(byte_vector));
  u8 *bytes = (u8*)&memory.the_objects[UnboxReference(byte_vector)+1];
  bytes[index] = value;
}

void PrintByteVector(Object object) {
  printf("(byte-vector");
  u64 reference = UnboxReference(object);
  u8 *bytes = (u8*)&memory.the_objects[reference+1];
  s64 length = UnsafeByteVectorLength(object);
  for (s64 index = 0; index < length; ++index) {
    printf(" 0x%x", bytes[index]);
  }
  printf(")");
}
