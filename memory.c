#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "blob.h"
#include "byte_vector.h"
#include "compound_procedure.h"
#include "log.h"
#include "pair.h"
#include "root.h"
#include "string.h"
#include "symbol.h"
#include "vector.h"

// Move an object from the_objects to new_objects.
Object MoveObject(Object object); 

// Functions for moving specific types of objects.
Object MovePrimitive(Object object);

// Print an object, not following references.
void PrintReference(Object object);
void PrintlnReference(Object object);

// Global memory storage
struct Memory memory;

void CollectGarbage() {
  ++memory.num_collections;
  LOG(LOG_MEMORY, "Beginning a garbage collection number %d\n", memory.num_collections);
  // DEBUGGING: Clear unused objects to nil
  for (u64 i = 0; i < memory.max_objects; ++i) memory.new_objects[i] = nil;
  LOG(LOG_MEMORY, "resetting the free pointer to 0\n");

  // Reset the free pointer to the start of the new_objects
  memory.free = 0;

  // Move the root from the_objects to new_objects.
  LOG(LOG_MEMORY, "Moving the root object: ");
  LOG_OP(LOG_MEMORY, PrintlnObject(memory.root));
  memory.root = MoveObject(memory.root);

  LOG(LOG_MEMORY, "Moved root. Free=%llu Beginning scan.\n", memory.free);
  // Scan over the freshly moved objects
  //  if a reference to an old object is encountered, move it
  // when scan catches up to free, the entire memory has been scanned/moved.
  for (u64 scan = 0; scan < memory.free;) {
    LOG(LOG_MEMORY, "Scanning object at %llu. Free=%llu\n", scan, memory.free);
    Object object = memory.new_objects[scan];

    // If the object is a blob, we can't scan its contents
    if (IsBlobHeader(object)) {
      u64 num_objects = NumObjectsPerBlob(UnboxBlobHeader(object));
      scan += num_objects;
      LOG(LOG_MEMORY, "Encountered blob of size %llu objects. Scan=%llu\n", num_objects, scan);
    } else {
      memory.new_objects[scan] = MoveObject(object);
      ++scan;
    }
  }
  memory.num_objects_moved += memory.free;

  // Flip
  Object *temp = memory.the_objects;
  memory.the_objects = memory.new_objects;
  memory.new_objects = temp;
}


Object MoveObject(Object object) {
  LOG(LOG_MEMORY, "moving object: ");
  LOG_OP(LOG_MEMORY, PrintlnReference(object));
  // If it isn't tagged, it's a double float.
  if (!IsTagged(object)) {
    return MovePrimitive(object);
  }
  switch (GetTag(object)) {
    case TAG_NIL:
    case TAG_TRUE:
    case TAG_FALSE:
    case TAG_FIXNUM:
    case TAG_REAL32:
    case TAG_PRIMITIVE_PROCEDURE:
      return MovePrimitive(object);

    // Reference Objects
    case TAG_PAIR:               return MovePair(object);
    case TAG_STRING:             return MoveString(object);
    case TAG_SYMBOL:             return MoveSymbol(object);
    case TAG_VECTOR:             return MoveVector(object);
    case TAG_BYTE_VECTOR:        return MoveByteVector(object);
    case TAG_COMPOUND_PROCEDURE: return MoveCompoundProcedure(object);
  }

  assert(!"Error: unrecognized object");
  return 0;
}

Object MovePrimitive(Object object) { return object; }

void InitializeMemory(u64 max_objects, enum ErrorCode *error) {
  memory.max_objects = max_objects;
  memory.num_collections = 0;
  memory.num_objects_allocated = 0;
  memory.num_objects_moved = 0;
  u64 n_bytes = sizeof(Object)*memory.max_objects;

  memory.the_objects = (Object*)malloc(n_bytes);
  if (!memory.the_objects) {
    *error = ERROR_COULD_NOT_ALLOCATE_HEAP;
    return;
  }
  assert(memory.the_objects);

  memory.new_objects = (Object*)malloc(n_bytes);
  if (!memory.the_objects) {
    *error = ERROR_COULD_NOT_ALLOCATE_HEAP_BUFFER;
    return;
  }
  assert(memory.new_objects);

  for (u64 i = 0; i < memory.max_objects; ++i) memory.the_objects[i] = nil;
  memory.free = 0;

  InitializeRoot(error);
}

void DestroyMemory() {
  free(memory.the_objects);
  free(memory.new_objects);
}

b64 HasEnoughMemory(u64 num_objects_required) {
  return memory.free + num_objects_required <= memory.max_objects;
}

void EnsureEnoughMemory(u64 num_objects_required, enum ErrorCode *error) {
  if (!HasEnoughMemory(num_objects_required)) {
    CollectGarbage();
  }

  if (!HasEnoughMemory(num_objects_required)) {
    *error = ERROR_OUT_OF_MEMORY;
  }
}

void PrintObject(Object object) {
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
    case TAG_PRIMITIVE_PROCEDURE: printf("<procedure %x>", UnboxPrimitiveProcedure(object)); break;

    // Reference Objects
    case TAG_PAIR:               PrintPair(object);              break;
    case TAG_VECTOR:             PrintVector(object);            break;
    case TAG_STRING:             PrintString(object);            break;
    case TAG_SYMBOL:             PrintSymbol(object);            break;
    case TAG_BYTE_VECTOR:        PrintByteVector(object);        break;
    case TAG_COMPOUND_PROCEDURE: PrintCompoundProcedure(object); break;
  }
}

void PrintlnObject(Object object) {
  PrintObject(object);
  printf("\n");
}

void PrintlnReference(Object object) {
  PrintReference(object);
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
      case TAG_REAL32: printf( "%ff", UnboxReal32(object)); break;
      // Reference Objects
      case TAG_PAIR:               printf("<Pair %llu>",              UnboxReference(object)); break;
      case TAG_STRING:             printf("<String %llu>",            UnboxReference(object)); break;
      case TAG_SYMBOL:             printf("<Symbol %llu>",            UnboxReference(object)); break;
      case TAG_VECTOR:             printf("<Vector %llu>",            UnboxReference(object)); break;
      case TAG_BYTE_VECTOR:        printf("<ByteVector %llu>",        UnboxReference(object)); break;
      case TAG_COMPOUND_PROCEDURE: printf("<CompoundProcedure %llu>", UnboxReference(object)); break;
    }
  }
}

void PrintMemory() {
  printf("Free=%llu, Root=", memory.free);
  PrintlnObject(memory.root);
  printf("0:");
  const int width = 8;
  for (u64 i = 0; i < memory.max_objects; ++i) {
    if (i > 0 && i % width == 0) printf(" |\n%d:", i);
    printf(" | ");
    PrintReference(memory.the_objects[i]);
  }
  printf(" |\n");
}

// Unsafe, only for testing
static Object MakePair(Object car, Object cdr, enum ErrorCode *error) {
  Object pair = AllocatePair(error);
  assert(!*error);
  // REFERENCES INVALIDATED
  SetCar(pair, car);
  SetCdr(pair, cdr);
  return pair;
}

void TestMemory() {
  enum ErrorCode error = NO_ERROR;
  InitializeMemory(32, &error);
  MakePair(BoxFixnum(4), BoxFixnum(2), &error);
  Object string = AllocateString("Hello", &error);

  Object vector = AllocateVector(3, &error);
  VectorSet(vector, 0, AllocateString("Zero", &error), &error);
  VectorSet(vector, 1, AllocateString("One", &error), &error);
  VectorSet(vector, 2, AllocateString("Two", &error), &error);

  Object byte_vector = AllocateByteVector(4, &error);
  ByteVectorSet(byte_vector, 0, 0xc, &error);
  ByteVectorSet(byte_vector, 1, 0xa, &error);
  ByteVectorSet(byte_vector, 2, 0xf, &error);
  ByteVectorSet(byte_vector, 3, 0xe, &error);

  Object shared = MakePair(byte_vector, string, &error);
  SetRegister(REGISTER_EXPRESSION, MakePair(shared, MakePair(shared, vector, &error), &error));

  LOG(LOG_TEST, "Old Root: ");
  LOG_OP(LOG_TEST, PrintlnObject(GetRegister(REGISTER_EXPRESSION)));
  LOG_OP(LOG_TEST, PrintMemory());
  CollectGarbage();

  LOG(LOG_TEST, "New Root: ");
  LOG_OP(LOG_TEST, PrintlnObject(GetRegister(REGISTER_EXPRESSION)));
  LOG_OP(LOG_TEST, PrintMemory());

  for (int i = 0; i < 1000; ++i) {
    MakePair(BoxFixnum(0), BoxFixnum(1), &error);
  }
  LOG(LOG_TEST, "Root: ");
  LOG_OP(LOG_TEST, PrintlnObject(GetRegister(REGISTER_EXPRESSION)));
  LOG(LOG_TEST, "Allocated %llu objects, performed %llu garbage collections, moved %llu objects,\n"
      "on average: %llf objects allocated/collection, %llf objects moved/collection\n",
      memory.num_objects_allocated, memory.num_collections, memory.num_objects_moved,
      memory.num_objects_allocated * 1.0 / memory.num_collections,
      memory.num_objects_moved * 1.0 / memory.num_collections);

  SetRegister(REGISTER_EXPRESSION, nil);
  SetRegister(REGISTER_EXPRESSION, AllocateVector(30 - NUM_REGISTERS, &error));
  LOG(LOG_TEST, "Root: ");
  LOG_OP(LOG_TEST, PrintlnObject(GetRegister(REGISTER_EXPRESSION)));
  DestroyMemory();
}
