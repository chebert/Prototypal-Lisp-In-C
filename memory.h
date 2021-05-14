#ifndef MEMORY_H
#define MEMORY_H

#include "tag.h"

// Memory Layouts:
//
// Before Moving:
// TYPE     | SIZE in Objects        | LAYOUT                                                 | LAYOUT after Moving           |
// ---------|------------------------|--------------------------------------------------------|-------------------------------|
// PRIMITIVE|(1)                     |[ ..., primitive, ...]                                  |[ ..., primitive, ...]         |
// REFERENCE|(1)                     |[ ..., reference, ...]                                  |[ ..., reference, ...]         |
// PAIR     |(2)                     |[ ..., car, cdr, ... ]                                  |[ ..., <BH new>, cdr ]         |
// VECTOR   |(nObjects+1)            |[ ..., nObjects, object0, object1, .., objectN, ... ]   |[ ..., <BH new>, object0, ... ]|
// BLOB     |(ceiling(nBytes, 8) + 1)|[ ..., nBytes, byte0, byte1, .., byteN, pad.., ... ]    |[ ..., <BH new>, byte0, ... ]  |

// Blobs are padded to the nearest Object boundary.
// <BH new>: A Broken Heart points to the newly allocated structure at index new.

// TODO: A garbage collector which has 2 live regions instead of 1
//  - Large: contains large & old objects
//  - Small: contains small & new objects
// When the small runs out of memory, a collection occurs.
//   Objects in the small region are moved to the large region.
//   Objects in the large region are not moved.
// When the large runs out of memory, a collection occurs.
//   Objects in the large region are moved.
// This is to reduce the size of moves, since most objects are short-lived and small.

// TODO: Weak References

struct Memory {
  // The live objects
  Object *the_objects;
  // The swap buffer for use during collection.
  Object *new_objects;

  // Index to the first free Object in the_objects.
  u64 free;
  // The root object.
  Object root;
  // The maximum number of objects which can be allocated in memory.
  u64 max_objects;

  // The number of times a Garbage collection has been performed.
  u64 num_collections;
  // The total number of object allocations that have occurred
  u64 num_objects_allocated;
  // The total number of objects that have been copied due to GCs.
  u64 num_objects_moved;
};

// Allocate memory needed to store up to max_objects.
struct Memory InitializeMemory(u64 max_objects, u64 symbol_table_size);
void DestroyMemory(struct Memory *memory);

// Perform a compacting garbage collection on the objects in memory
void CollectGarbage(struct Memory *memory);

// Performs a garbage collection if there isn't enough memory.
// If there still isn't enough memory, causes an exception.
void EnsureEnoughMemory(struct Memory *memory, u64 num_objects_required);

// Warning: Every time you Allocate, all references in C code may be invalid.

// Print an object, following references.
void PrintObject(struct Memory *memory, Object object);
// Print an object, following references, followed by a newline.
void PrintlnObject(struct Memory *memory, Object object);
// Print an object, not following references.
void PrintReference(Object object);

void TestMemory();

#endif
