#ifndef BYTE_VECTOR_H
#define BYTE_VECTOR_H

#include "tag.h"

// A byte vector is an array of 8-bit unsigned bytes.
// It is implemented as a Blob.

// Allocate and access a vector of 8-bit bytes.
Object AllocateByteVector(u64 num_bytes);
Object MoveByteVector(u64 ref);
Object ByteVectorRef(u64 reference, u64 index);
void ByteVectorSet(u64 reference, u64 index, u8 value);
void PrintByteVector(Object object);

#endif
