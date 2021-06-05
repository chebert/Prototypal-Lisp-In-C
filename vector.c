#include "vector.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "memory.h"

Object AllocateVector(u64 num_objects, enum ErrorCode *error) {
  EnsureEnoughMemory(num_objects + 1, error);
  if (*error) {
    LOG_ERROR("Not enough memory to allocate vector of size %llu. %s", num_objects);
    return nil;
  }
  // [ ..., free.. ]
  u64 new_reference = memory.free;

  memory.the_objects[memory.free++] = BoxFixnum(num_objects);
  for (int i = 0; i < num_objects; ++i)
    memory.the_objects[memory.free++] = nil;
  memory.num_objects_allocated += num_objects + 1;
  // [ ..., nObjects, Object0, ..., ObjectN, free.. ]

  return BoxVector(new_reference);
}

Object MoveVector(Object vector) {
  u64 ref = UnboxReference(vector);
  // New: [ ..., free... ]
  // Old: [ ..., nObjects, Object0, ... ObjectN, ... ] OR
  //      [ ..., <BH new>, ... ]
  u64 new_reference = memory.free;

  LOG(LOG_MEMORY, "moving from %llu in the_objects to %llu in new_objects\n", ref, new_reference);
  Object old_header = memory.the_objects[ref];

  if (IsBrokenHeart(old_header)) {
    // Already been moved
    // Old: [ ..., <BH new>, ... ]
    LOG(LOG_MEMORY, "old_header is a broken heart pointing to %llu\n", UnboxReference(old_header));
    return BoxVector(UnboxReference(old_header));
  }

  LOG_OP(LOG_MEMORY, PrintlnObject(old_header));
  assert(IsFixnum(old_header));
  // Old: [ ..., nObjects, Object0, ... ObjectN, ... ]

  u64 num_objects = 1 + UnboxFixnum(old_header);
  LOG(LOG_MEMORY, "moving vector of size %llu objects (including header)\n", num_objects);

  memcpy(&memory.new_objects[memory.free], &memory.the_objects[ref], num_objects*sizeof(Object));
  memory.free += num_objects;
  // New: [ ..., nObjects, Object0, ... ObjectN, free.. ]

  LOG(LOG_MEMORY, "Leaving a broken heart pointing at %llu in its place\n", new_reference);
  memory.the_objects[ref] = BoxBrokenHeart(new_reference);
  // Old: [ ..., <BH new>, ... ]

  return BoxVector(new_reference);
}

s64 UnsafeVectorLength(Object vector) {
  assert(IsVector(vector));
  return UnboxFixnum(memory.the_objects[UnboxReference(vector)]);
}
Object UnsafeVectorRef(Object vector, u64 index) {
  assert(IsVector(vector));
  assert(index < UnsafeVectorLength(vector));
  return memory.the_objects[UnboxReference(vector)+1 + index];
}
void UnsafeVectorSet(Object vector, u64 index, Object value) {
  assert(IsVector(vector));
  assert(index < UnsafeVectorLength(vector));
  memory.the_objects[UnboxReference(vector)+1 + index] = value;
}

void PrintVector(Object vector) {
  assert(IsVector(vector));
  u64 reference = UnboxReference(vector);
  u64 length = UnboxFixnum(memory.the_objects[reference]);
  printf("(vector");
  for (u64 index = 0; index < length; ++index) {
    printf(" ");
    PrintObject(memory.the_objects[reference+1 + index]);
  }
  printf(")");
}
