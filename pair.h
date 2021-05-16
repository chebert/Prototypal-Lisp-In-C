#ifndef PAIR_H
#define PAIR_H

#include "tag.h"

// A pair is a compound type representing 2 associated Objects.
// Memory Layout: [ ..., car, cdr, ...]
//
// A pair Object is a reference to this pair.

// Allocate a pair of 2 objects.
Object AllocatePair();
// Move a pair from the_objects to new_objects
Object MovePair(u64 ref);

Object Car(u64 reference);
Object Cdr(u64 reference);
void SetCar(u64 reference, Object value);
void SetCdr(u64 reference, Object value);
// Safely allocate a pair and set the car and cdr.
Object MakePair(Object car, Object cdr);

void PrintPair(Object pair);

#endif
