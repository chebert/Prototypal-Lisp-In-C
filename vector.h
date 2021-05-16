#ifndef VECTOR_H
#define VECTOR_H

#include "tag.h"

// A vector is a fixed-length 1-dimensional array of Objects, with O(1) random access.
// Memory Layout: [ ..., N, Object0, Object1, .., ObjectN, ...]

// Allocate and access a vector of objects.
Object AllocateVector(u64 num_objects);

Object MoveVector(Object vector);

s64 VectorLength(Object vector);
Object VectorRef(Object vector, u64 index);
void VectorSet(Object vector, u64 index, Object value);
void PrintVector(Object vector);


#endif
