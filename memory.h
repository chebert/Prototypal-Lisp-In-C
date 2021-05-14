#ifndef MEMORY_H
#define MEMORY_H

#include "tag.h"

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

// Allocate and access a vector of objects.
Object AllocateVector(struct Memory *memory, u64 num_objects);
s64 VectorLength(struct Memory *memory, u64 reference);
Object VectorRef(struct Memory *memory, u64 reference, u64 index);
void VectorSet(struct Memory *memory, u64 reference, u64 index, Object value);

// Allocate and access a vector of 8-bit bytes.
Object AllocateByteVector(struct Memory *memory, u64 num_bytes);
Object ByteVectorRef(struct Memory *memory, u64 reference, u64 index);
void ByteVectorSet(struct Memory *memory, u64 reference, u64 index, u8 value);

// Allocate a string of characters.
Object AllocateString(struct Memory *memory, const char *string);
Object AllocateSymbol(struct Memory *memory, const char *name);

// Allocate a pair of 2 objects.
Object AllocatePair(struct Memory *memory);
Object Car(struct Memory *memory, u64 reference);
Object Cdr(struct Memory *memory, u64 reference);
void SetCar(struct Memory *memory, u64 reference, Object value);
void SetCdr(struct Memory *memory, u64 reference, Object value);

// Print an object, following references.
void PrintObject(struct Memory *memory, Object object);
// Print an object, following references, followed by a newline.
void PrintlnObject(struct Memory *memory, Object object);
// Print an object, not following references.
void PrintReference(Object object);

void TestMemory();

#endif
