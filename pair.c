#include "pair.h"

#include <assert.h>
#include <stdio.h>

#include "log.h"
#include "memory.h"
#include "root.h"

// TODO: Fixed-size Allocations (Pair, Procedure)
Object AllocatePair(enum ErrorCode *error) {
  EnsureEnoughMemory(2, error);
  if (*error) {
    LOG_ERROR("Not enough memory to allocate pair");
    return nil;
  }

  // [ ..., free.. ]
  u64 new_reference = memory.free;
  memory.the_objects[memory.free++] = nil;
  memory.the_objects[memory.free++] = nil;
  memory.num_objects_allocated += 2;
  // [ ..., car, cdr, free.. ]
  return BoxPair(new_reference);
}

Object MovePair(Object pair) {
  u64 ref = UnboxReference(pair);
  // New: [ ..., free... ]
  // Old: [ ..., car, cdr, ...] OR
  //      [ ..., <BH new>, ... ]
  u64 new_reference = memory.free;

  LOG(LOG_MEMORY, "moving from %llu in the_objects to %llu in new_objects\n", ref, new_reference);
  Object old_car = memory.the_objects[ref];
  if (IsBrokenHeart(old_car)) {
    // The pair has already been moved. Return the updated reference.
    // Old: [ ..., <BH new>, ... ]
    LOG(LOG_MEMORY, "old_car is a broken heart pointing to %llu\n", UnboxReference(old_car));
    return BoxPair(UnboxReference(old_car));
  }

  LOG(LOG_MEMORY, "Moving from the_objects to new_objects, leaving a broken heart at %llu pointing to %llu\n", ref, new_reference);
  Object moved_pair = BoxPair(new_reference);
  // Old: [ ..., car, cdr, ... ]
  memory.new_objects[memory.free++] = old_car;
  memory.new_objects[memory.free++] = memory.the_objects[ref+1];
  // New: [ ..., car, cdr, free.. ]
  memory.the_objects[ref] = BoxBrokenHeart(new_reference);
  // Old: [ ...,  <BH new>, ... ]
  return moved_pair;
}

Object Car(Object pair) {
  assert(IsPair(pair));
  return memory.the_objects[UnboxReference(pair)];
}
Object Cdr(Object pair) {
  assert(IsPair(pair));
  return memory.the_objects[UnboxReference(pair) + 1];
}

void SetCar(Object pair, Object value) {
  assert(IsPair(pair));
  memory.the_objects[UnboxReference(pair)] = value;
}
void SetCdr(Object pair, Object value) {
  assert(IsPair(pair));
  memory.the_objects[UnboxReference(pair) + 1] = value;
}

Object First(Object pair) { return Car(pair); }
Object Rest(Object pair) { return Cdr(pair); }

void PrintPair(Object pair) {
  u64 reference = UnboxReference(pair);
  printf("(");
  PrintObject(Car(pair));

  Object rest = Cdr(pair);
  if (IsPair(rest)) {
    for (; IsPair(rest); rest = Cdr(rest)) {
      printf(" ");
      PrintObject(Car(rest));
    }
  }

  if (IsNil(rest)) {
    printf(")");
  } else {
    printf(" . ");
    PrintObject(rest);
    printf(")");
  }
}
