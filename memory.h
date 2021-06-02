#ifndef MEMORY_H
#define MEMORY_H

#include "error.h"
#include "tag.h"

// Memory is managed using a stop-and-copy garbage collection algorithm.
//
// Root references the live objects in the system. Anything not reachable from the root is considered garbage.
// Memory consists of two regions:
//   the_objects: a mixture the current (live) objects and discared (garbage) objects in the system.
//   new_objects: during GC, live/reachable objects are moved from the_objects into new_objects.
//      when complete, the new_objects and the_objects are swapped
//
// A Garbage Collection occurs when the system attempts to Allocate and there is not enough memory.
// If there is still not enough memory after GC, the system fails.
// A collection can be invoked early with CollectGarbage();

// Memory Layouts During GC:
//
// Memory layouts in the_objects before and after moving:
// TYPE     | SIZE in Objects        | LAYOUT before Moving                                   | LAYOUT after Moving           |
// ---------|------------------------|--------------------------------------------------------|-------------------------------|
// PRIMITIVE| 1                      |[ ..., primitive, ...]                                  |[ ..., primitive, ...]         |
// REFERENCE| 1                      |[ ..., reference, ...]                                  |[ ..., reference, ...]         |
// PAIR     | 2                      |[ ..., car, cdr, ... ]                                  |[ ..., <BH new>, cdr ]         |
// VECTOR   | nObjects+1             |[ ..., nObjects, object0, object1, .., objectN, ... ]   |[ ..., <BH new>, object0, ... ]|
// BLOB     | ceiling(nBytes, 8) + 1 |[ ..., nBytes, byte0, byte1, .., byteN, pad.., ... ]    |[ ..., <BH new>, byte0, ... ]  |

// Blobs are padded to the nearest Object boundary.
// <BH new>: A Broken Heart points to the newly moved structure in new_objects at index new.

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
extern struct Memory memory;

// Allocate memory needed to store up to max_objects.
void InitializeMemory(u64 max_objects, enum ErrorCode *error);
void DestroyMemory();

// Perform a compacting garbage collection on the objects in memory
void CollectGarbage();

// Performs a garbage collection if there isn't enough memory.
// If there still isn't enough memory, returns an out of memory error.
void EnsureEnoughMemory(u64 num_objects_required, enum ErrorCode *error);

// Warning: Every time you Allocate, all references in C code may be invalid.

// Print an object, following references.
void PrintObject(Object object);
// Print an object, following references, followed by a newline.
void PrintlnObject(Object object);

void TestMemory();

#endif
