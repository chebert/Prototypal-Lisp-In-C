#ifndef BYTE_VECTOR_H
#define BYTE_VECTOR_H

#include "tag.h"

// A byte vector is an array of 8-bit unsigned bytes.
// It is implemented as a Blob.

// Allocate and access a vector of 8-bit bytes.
Object AllocateByteVector(u64 num_bytes);
Object MoveByteVector(Object byte_vector);

// Returns the number of bytes in byte_vector.
s64 ByteVectorLength(Object byte_vector);

// Returns a fixnum representing the byte at the given 0-based index
Object ByteVectorRef(Object byte_vector, u64 index);

// Sets the byte at the given 0-based index to value.
void ByteVectorSet(Object byte_vector, u64 index, u8 value);

void PrintByteVector(Object object);

#endif
