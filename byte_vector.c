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

Object MoveByteVector(Object byte_vector) {
  return BoxByteVector(MoveBlob(UnboxReference(byte_vector)));
}

s64 UnsafeByteVectorLength(Object byte_vector) {
  assert(IsByteVector(byte_vector));
  return UnboxFixnum(memory.the_objects[UnboxReference(byte_vector)]);
}

s64 ByteVectorLength(Object byte_vector, enum ErrorCode *error) {
  if (!IsByteVector(byte_vector)) {
    *error = ERROR_BYTE_VECTOR_LENGTH_NON_BYTE_VECTOR;
    return 0;
  }

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

Object ByteVectorRef(Object byte_vector, u64 index, enum ErrorCode *error) {
  if (!IsByteVector(byte_vector)) {
    *error = ERROR_BYTE_VECTOR_REFERENCE_NON_BYTE_VECTOR;
    return nil;
  }
  if (index >= UnsafeByteVectorLength(byte_vector)) {
    *error = ERROR_BYTE_VECTOR_REFERENCE_INDEX_OUT_OF_RANGE;
    return nil;
  }
  *error = NO_ERROR;
  return UnsafeByteVectorRef(byte_vector, index);
}

void ByteVectorSet(Object byte_vector, u64 index, u8 value, enum ErrorCode *error) {
  if (!IsByteVector(byte_vector)) {
    *error = ERROR_BYTE_VECTOR_SET_NON_BYTE_VECTOR;
    return;
  }
  if (index >= UnsafeByteVectorLength(byte_vector)) {
    *error = ERROR_BYTE_VECTOR_SET_INDEX_OUT_OF_RANGE;
    return;
  }
  *error = NO_ERROR;
  UnsafeByteVectorSet(byte_vector, index, value);
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
