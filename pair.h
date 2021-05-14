#ifndef PAIR_H
#define PAIR_H

#include "memory.h"

// A pair is a compound type representing 2 associated Objects.
// Memory Layout: [ ..., car, cdr, ...]
//
// A pair Object is a reference to this pair.

// Allocate a pair of 2 objects.
Object AllocatePair(struct Memory *memory);
// Move a pair from the_objects to new_objects
Object MovePair(struct Memory *memory, u64 ref);

Object Car(struct Memory *memory, u64 reference);
Object Cdr(struct Memory *memory, u64 reference);
void SetCar(struct Memory *memory, u64 reference, Object value);
void SetCdr(struct Memory *memory, u64 reference, Object value);
// Safely allocate a pair and set the car and cdr.
Object MakePair(struct Memory *memory, Object car, Object cdr);

void PrintPair(struct Memory *memory, Object pair);

#endif
