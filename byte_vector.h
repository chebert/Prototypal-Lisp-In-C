#ifndef BYTE_VECTOR_H
#define BYTE_VECTOR_H

#include "memory.h"

// Allocate and access a vector of 8-bit bytes.
Object AllocateByteVector(struct Memory *memory, u64 num_bytes);
Object MoveByteVector(struct Memory *memory, u64 ref);
Object ByteVectorRef(struct Memory *memory, u64 reference, u64 index);
void ByteVectorSet(struct Memory *memory, u64 reference, u64 index, u8 value);
void PrintByteVector(struct Memory *memory, Object object);

#endif
