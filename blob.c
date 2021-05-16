#include "blob.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"

// Returns ceiling(numerator/denominator)
u64 CeilingU64(u64 numerator, u64 denominator);


u64 AllocateBlob(u64 num_bytes) {
  u64 num_objects = NumObjectsPerBlob(num_bytes);
  EnsureEnoughMemory(num_objects);
  // [ ..., free.. ]

  u64 new_reference = memory.free;
  memory.the_objects[new_reference] = BoxBlobHeader(num_bytes);
  memory.free += num_objects;
  memory.num_objects_allocated += num_objects;
  // [ ..., nBytes, byte0, ..., byteN, pad.., free.. ]

  return new_reference;
}

u64 MoveBlob(u64 ref) {
  // New: [ ..., free... ]
  // Old: [ ..., nBytes, byte0, ..., byteN, pad.., ] OR
  //      [ ..., <BH new>, ... ]
  u64 new_reference = memory.free;

  printf("    MoveBlob: moving from %llu in the_objects to %llu in new_objects\n", ref, new_reference);
  Object old_header = memory.the_objects[ref];
  if (IsBrokenHeart(old_header)) {
    // Already been moved
    // Old: [ ..., <BH new>, ... ]
    printf("    MoveBlob: old_header is a broken heart pointing to %llu\n", UnboxReference(old_header));
    return UnboxReference(old_header);
  }

  // Old: [ ..., nBytes, byte0, ..., byteN, pad.., ... ]
  assert(IsBlobHeader(old_header));
  u64 bytes_in_blob = UnboxBlobHeader(old_header);
  u64 num_objects = NumObjectsPerBlob(bytes_in_blob);
  printf("    MoveBlob: moving blob of size %llu bytes, (%llu objects)\n", bytes_in_blob, num_objects);

  memcpy(&memory.new_objects[memory.free], &memory.the_objects[ref], num_objects*sizeof(Object));
  memory.free += num_objects;
  // New: [ ..., nBytes, byte0, ..., byteN, pad.., free.. ]

  printf("    MoveBlob: Leaving a broken heart pointing at %llu in its place\n", new_reference);
  memory.the_objects[ref] = BoxBrokenHeart(new_reference);
  // Old: [ ..., <BH new>, ... ]

  return new_reference;
}


u64 NumObjectsPerBlob(u64 bytes_in_blob) {
  // number of objects required to hold at least bytes_in_blob + 1 for the header
  return 1 + CeilingU64(bytes_in_blob, sizeof(Object));
}


// Returns ceiling(numerator/denominator)
u64 CeilingU64(u64 numerator, u64 denominator) {
  return (numerator + denominator - 1) / denominator;
}
