#include "vector.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

Object AllocateVector(struct Memory *memory, u64 num_objects) {
  EnsureEnoughMemory(memory, num_objects + 1);
  // [ ..., free.. ]
  u64 new_reference = memory->free;

  memory->the_objects[memory->free++] = BoxFixnum(num_objects);
  for (int i = 0; i < num_objects; ++i)
    memory->the_objects[memory->free++] = nil;
  memory->num_objects_allocated += num_objects + 1;
  // [ ..., nObjects, Object0, ..., ObjectN, free.. ]

  return BoxVector(new_reference);
}

Object MoveVector(struct Memory *memory, u64 ref) {
  // New: [ ..., free... ]
  // Old: [ ..., nObjects, Object0, ... ObjectN, ... ] OR
  //      [ ..., <BH new>, ... ]
  u64 new_reference = memory->free;

  printf("    MoveVector: moving from %llu in the_objects to %llu in new_objects\n", ref, new_reference);
  Object old_header = memory->the_objects[ref];

  if (IsBrokenHeart(old_header)) {
    // Already been moved
    // Old: [ ..., <BH new>, ... ]
    printf("    MoveVector: old_header is a broken heart pointing to %llu\n", UnboxReference(old_header));
    return BoxVector(UnboxReference(old_header));
  }

  assert(IsFixnum(old_header));
  // Old: [ ..., nObjects, Object0, ... ObjectN, ... ]

  u64 num_objects = 1 + UnboxFixnum(old_header);
  printf("    MoveVector: moving vector of size %llu objects (including header)\n", num_objects);

  memcpy(&memory->new_objects[memory->free], &memory->the_objects[ref], num_objects*sizeof(Object));
  memory->free += num_objects;
  // New: [ ..., nObjects, Object0, ... ObjectN, free.. ]

  printf("    MoveVector: Leaving a broken heart pointing at %llu in its place\n", new_reference);
  memory->the_objects[ref] = BoxBrokenHeart(new_reference);
  // Old: [ ..., <BH new>, ... ]

  return BoxVector(new_reference);
}

s64 VectorLength(struct Memory *memory, u64 reference) {
  return UnboxFixnum(memory->the_objects[reference]);
}
Object VectorRef(struct Memory *memory, u64 reference, u64 index) {
  assert(index < VectorLength(memory, reference));
  return memory->the_objects[reference+1 + index];
}
void VectorSet(struct Memory *memory, u64 reference, u64 index, Object value) {
  assert(index < VectorLength(memory, reference));
  memory->the_objects[reference+1 + index] = value;
}

void PrintVector(struct Memory *memory, Object vector) {
  u64 reference = UnboxReference(vector);
  u64 length = UnboxFixnum(memory->the_objects[reference]);
  printf("(vector");
  for (u64 index = 0; index < length; ++index) {
    printf(" ");
    PrintObject(memory, memory->the_objects[reference+1 + index]);
  }
  printf(")");
}