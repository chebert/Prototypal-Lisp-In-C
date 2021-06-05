#ifndef BYTE_VECTOR_H
#define BYTE_VECTOR_H

#include "error.h"
#include "tag.h"

// A byte vector is an array of 8-bit unsigned bytes.
// It is implemented as a Blob.

// Allocate and access a vector of 8-bit bytes.
Object AllocateByteVector(u64 num_bytes, enum ErrorCode *error);
Object MoveByteVector(Object byte_vector);

// Returns the number of bytes in byte_vector.
// Crashes if byte_vector isn't a byte vector.
s64 UnsafeByteVectorLength(Object byte_vector);

// Returns a fixnum representing the byte at the given 0-based index
// Crashes if index is out of range or byte_vector is not a byte vector.
Object UnsafeByteVectorRef(Object byte_vector, u64 index);

// Sets the byte at the given 0-based index to value.
// Crashes if index is out of range or byte_vector is not a byte vector.
void UnsafeByteVectorSet(Object byte_vector, u64 index, u8 value);

void PrintByteVector(Object object);

#endif
