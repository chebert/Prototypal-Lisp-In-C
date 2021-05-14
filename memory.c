#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "blob.h"
#include "byte_vector.h"
#include "pair.h"
#include "root.h"
#include "string.h"
#include "symbol.h"
#include "vector.h"

// Move an object from the_objects to new_objects.
Object MoveObject(struct Memory *memory, Object object); 

// Functions for moving specific types of objects.
Object MovePrimitive(Object object);

// Print an object, not following references.
void PrintReference(Object object);

void CollectGarbage(struct Memory *memory) {
  ++memory->num_collections;
  printf("CollectGarbage: Beginning a garbage collection number %d\n", memory->num_collections);
  // DEBUGGING: Clear unused objects to nil
  for (u64 i = 0; i < memory->max_objects; ++i) memory->new_objects[i] = nil;
  printf("CollectGarbage: resetting the free pointer to 0\n");

  // Reset the free pointer to the start of the new_objects
  memory->free = 0;

  // Move the root from the_objects to new_objects.
  printf("CollectGarbage: Moving the root object: ");
  PrintlnObject(memory, memory->root);
  memory->root = MoveObject(memory, memory->root);

  printf("CollectGarbage: Moved root. Free=%llu Beginning scan.\n", memory->free);
  // Scan over the freshly moved objects
  //  if a reference to an old object is encountered, move it
  // when scan catches up to free, the entire memory has been scanned/moved.
  for (u64 scan = 0; scan < memory->free; ++scan) {
    printf("CollectGarbage: Scanning object at %llu. Free=%llu\n", scan, memory->free);
    memory->new_objects[scan] = MoveObject(memory, memory->new_objects[scan]);
  }
  memory->num_objects_moved += memory->free;

  // Flip
  Object *temp = memory->the_objects;
  memory->the_objects = memory->new_objects;
  memory->new_objects = temp;
}


Object MoveObject(struct Memory *memory, Object object) {
  printf("  MoveObject: moving object: ");
  PrintReference(object);
  printf("\n");
  // If it isn't tagged, it's a double float.
  if (!IsTagged(object)) { return MovePrimitive(object); }
  switch (GetTag(object)) {
    case TAG_NIL:
    case TAG_TRUE:
    case TAG_FALSE:
    case TAG_FIXNUM:
    case TAG_REAL32:
      return MovePrimitive(object);

    // Reference Objects
    case TAG_PAIR:        return       MovePair(memory, UnboxReference(object));
    case TAG_STRING:      return     MoveString(memory, UnboxReference(object));
    case TAG_SYMBOL:      return     MoveSymbol(memory, UnboxReference(object));
    case TAG_VECTOR:      return     MoveVector(memory, UnboxReference(object));
    case TAG_BYTE_VECTOR: return MoveByteVector(memory, UnboxReference(object));
  }

  assert(!"Error: unrecognized object");
  return 0;
}

Object MovePrimitive(Object object) { return object; }

struct Memory InitializeMemory(u64 max_objects, u64 symbol_table_size) {
  struct Memory memory; 
  memory.max_objects = max_objects;
  memory.num_collections = 0;
  memory.num_objects_allocated = 0;
  memory.num_objects_moved = 0;
  u64 n_bytes = sizeof(Object)*memory.max_objects;

  memory.the_objects = (Object*)malloc(n_bytes);
  assert(memory.the_objects);

  memory.new_objects = (Object*)malloc(n_bytes);
  assert(memory.new_objects);

  for (u64 i = 0; i < memory.max_objects; ++i) memory.the_objects[i] = nil;
  memory.free = 0;

  InitializeRoot(&memory, symbol_table_size);
  return memory;
}

void DestroyMemory(struct Memory *memory) {
  free(memory->the_objects);
  free(memory->new_objects);
}

void EnsureEnoughMemory(struct Memory *memory, u64 num_objects_required) {
  if (!(memory->free + num_objects_required <= memory->max_objects)) {
    CollectGarbage(memory);
  }
  assert(memory->free + num_objects_required <= memory->max_objects);
}

void PrintObject(struct Memory *memory, Object object) {
  if (IsReal64(object)) { 
    printf("%llf", UnboxReal64(object));
    return;
  }
  switch (GetTag(object)) {
    // Primitives
    case TAG_NIL:    printf("nil"); break;
    case TAG_TRUE:   printf("#t");  break;
    case TAG_FALSE:  printf("#f");  break;
    case TAG_FIXNUM: printf("%lld", UnboxFixnum(object)); break;
    case TAG_REAL32: printf("%ff",  UnboxReal32(object)); break;

    // Reference Objects
    case TAG_PAIR:              PrintPair(memory, object); break;
    case TAG_VECTOR:          PrintVector(memory, object); break;
    case TAG_STRING:          PrintString(memory, object); break;
    case TAG_SYMBOL:          PrintSymbol(memory, object); break;
    case TAG_BYTE_VECTOR: PrintByteVector(memory, object); break;
  }
}

void PrintlnObject(struct Memory *memory, Object object) {
  PrintObject(memory, object);
  printf("\n");
}

void PrintReference(Object object) {
  if (IsReal64(object)) {
    printf("%llf", UnboxReal64(object));
  } else {
    switch (GetTag(object)) {
      case TAG_NIL:    printf("nil");   break;
      case TAG_TRUE:   printf("true");  break;
      case TAG_FALSE:  printf("false"); break;
      case TAG_FIXNUM: printf("%lld", UnboxFixnum(object)); break;
      case TAG_REAL32: printf("%ff", UnboxReal32(object));  break;
      // Reference Objects
      case TAG_PAIR:        printf("<Pair %llu>",       UnboxReference(object)); break;
      case TAG_STRING:      printf("<String %llu>",     UnboxReference(object)); break;
      case TAG_SYMBOL:      printf("<Symbol %llu>",     UnboxReference(object)); break;
      case TAG_VECTOR:      printf("<Vector %llu>",     UnboxReference(object)); break;
      case TAG_BYTE_VECTOR: printf("<ByteVector %llu>", UnboxReference(object)); break;
    }
  }
}

void PrintMemory(struct Memory *memory) {
  printf("Free=%llu, Root=", memory->free);
  PrintlnObject(memory, memory->root);
  printf("0:");
  const int width = 8;
  for (u64 i = 0; i < memory->max_objects; ++i) {
    if (i > 0 && i % width == 0) printf(" |\n%d:", i);
    printf(" | ");
    PrintReference(memory->the_objects[i]);
  }
  printf(" |\n");
}

void TestMemory() {
  struct Memory memory = InitializeMemory(32, 0);
  MakePair(&memory, BoxFixnum(4), BoxFixnum(2));
  Object string = AllocateString(&memory, "Hello");

  Object vector = AllocateVector(&memory, 3);
  VectorSet(&memory, UnboxReference(vector), 0, AllocateString(&memory, "Zero"));
  VectorSet(&memory, UnboxReference(vector), 1, AllocateString(&memory, "One"));
  VectorSet(&memory, UnboxReference(vector), 2, AllocateString(&memory, "Two"));

  Object byte_vector = AllocateByteVector(&memory, 4);
  ByteVectorSet(&memory, UnboxReference(byte_vector), 0, 0xc);
  ByteVectorSet(&memory, UnboxReference(byte_vector), 1, 0xa);
  ByteVectorSet(&memory, UnboxReference(byte_vector), 2, 0xf);
  ByteVectorSet(&memory, UnboxReference(byte_vector), 3, 0xe);

  Object shared = MakePair(&memory, byte_vector, string);
  SetRegister(&memory, REGISTER_THE_OBJECT, MakePair(&memory, shared, MakePair(&memory, shared, vector)));

  printf("Old Root: ");
  PrintlnObject(&memory, GetRegister(&memory, REGISTER_THE_OBJECT));
  PrintMemory(&memory);
  CollectGarbage(&memory);

  printf("New Root: ");
  PrintlnObject(&memory, GetRegister(&memory, REGISTER_THE_OBJECT));
  PrintMemory(&memory);

  for (int i = 0; i < 1000; ++i) {
    MakePair(&memory, BoxFixnum(0), BoxFixnum(1));
  }
  printf("Root: ");
  PrintlnObject(&memory, GetRegister(&memory, REGISTER_THE_OBJECT));
  printf("Allocated %llu objects, performed %llu garbage collections, moved %llu objects,\n"
      "on average: %llf objects allocated/collection, %llf objects moved/collection\n",
      memory.num_objects_allocated, memory.num_collections, memory.num_objects_moved,
      memory.num_objects_allocated * 1.0 / memory.num_collections,
      memory.num_objects_moved * 1.0 / memory.num_collections);

  SetRegister(&memory, REGISTER_THE_OBJECT, nil);
  SetRegister(&memory, REGISTER_THE_OBJECT, AllocateVector(&memory, 25));
  printf("Root: ");
  PrintlnObject(&memory, GetRegister(&memory, REGISTER_THE_OBJECT));
}

