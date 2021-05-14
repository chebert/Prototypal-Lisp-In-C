#ifndef VECTOR_H
#define VECTOR_H

#include "memory.h"

// Allocate and access a vector of objects.
Object AllocateVector(struct Memory *memory, u64 num_objects);

Object MoveVector(struct Memory *memory, u64 ref);

s64 VectorLength(struct Memory *memory, u64 reference);
Object VectorRef(struct Memory *memory, u64 reference, u64 index);
void VectorSet(struct Memory *memory, u64 reference, u64 index, Object value);
void PrintVector(struct Memory *memory, Object vector);


#endif
