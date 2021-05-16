#ifndef VECTOR_H
#define VECTOR_H

#include "tag.h"

// A vector is a fixed-length 1-dimensional array of Objects, with O(1) random access.
// Memory Layout: [ ..., N, Object0, Object1, .., ObjectN, ...]

// Allocate and access a vector of objects.
Object AllocateVector(u64 num_objects);

Object MoveVector(u64 ref);

s64 VectorLength(u64 reference);
Object VectorRef(u64 reference, u64 index);
void VectorSet(u64 reference, u64 index, Object value);
void PrintVector(Object vector);


#endif
