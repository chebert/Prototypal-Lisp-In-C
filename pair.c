#include "pair.h"

#include <assert.h>
#include <stdio.h>

#include "memory.h"
#include "root.h"

Object AllocatePair() {
  EnsureEnoughMemory(2);

  // [ ..., free.. ]
  u64 new_reference = memory.free;
  memory.the_objects[memory.free++] = nil;
  memory.the_objects[memory.free++] = nil;
  memory.num_objects_allocated += 2;
  // [ ..., car, cdr, free.. ]
  return BoxPair(new_reference);
}

Object MovePair(u64 ref) {
  // New: [ ..., free... ]
  // Old: [ ..., car, cdr, ...] OR
  //      [ ..., <BH new>, ... ]
  u64 new_reference = memory.free;

  printf("    MovePair: moving from %llu in the_objects to %llu in new_objects\n", ref, new_reference);
  Object old_car = memory.the_objects[ref];
  if (IsBrokenHeart(old_car)) {
    // The pair has already been moved. Return the updated reference.
    // Old: [ ..., <BH new>, ... ]
    printf("    MovePair: old_car is a broken heart pointing to %llu\n", UnboxReference(old_car));
    return BoxPair(UnboxReference(old_car));
  }

  printf("    MovePair: Moving from the_objects to new_objects, leaving a broken heart at %llu pointing to %llu\n", ref, new_reference);
  Object the_pair = BoxPair(new_reference);
  // Old: [ ..., car, cdr, ... ]
  memory.new_objects[memory.free++] = old_car;
  memory.new_objects[memory.free++] = memory.the_objects[ref+1];
  memory.the_objects[ref] = BoxBrokenHeart(new_reference);
  // New: [ ..., car, cdr, free.. ]
  // Old: [ ...,  <BH new>, ... ]
  return the_pair;
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

Object MakePair(Object car, Object cdr) {
  SavePair(car, cdr);
  Object pair = AllocatePair();
  RestorePair(&car, &cdr);

  SetCar(pair, car);
  SetCdr(pair, cdr);

  return pair;
}

void PrintPair(Object pair) {
  u64 reference = UnboxReference(pair);
  printf("(");
  PrintObject(memory.the_objects[reference]);
  printf(" . ");
  PrintObject(memory.the_objects[reference+1]);
  printf(")");
}
