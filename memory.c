#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "memory.h"

// Performs a garbage collection if there isn't enough memory.
// If there still isn't enough memory, causes an exception.
void EnsureEnoughMemory(struct Memory *memory, u64 num_objects_required);

// Allocate a blob. Returns the index of the blob.
u64 AllocateBlob(struct Memory *memory, u64 num_bytes);
// Move an object from the_objects to new_objects.
Object MoveObject(struct Memory *memory, Object object); 

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

// Functions for moving specific types of objects.
Object MovePrimitive(Object object);
Object MovePair(struct Memory *memory, u64 ref);
Object MoveVector(struct Memory *memory, u64 ref);

// Returns an index into the array where the blob was moved to.
u64 MoveBlob(struct Memory *memory, u64 ref);
Object MoveString(struct Memory *memory, u64 ref);
Object MoveSymbol(struct Memory *memory, u64 ref);
Object MoveByteVector(struct Memory *memory, u64 ref);

// Returns ceiling(numerator/denominator)
u64 CeilingU64(u64 numerator, u64 denominator);
// Returns the number of Objects in a Blob (including header)
u64 NumObjectsPerBlob(u64 bytes_in_blob);

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

Object MovePrimitive(Object object) {return object; }

// Move References
Object MovePair(struct Memory *memory, u64 ref) {
  // New: [ ..., free... ]
  // Old: [ ..., car, cdr, ...] OR
  //      [ ..., <BH new>, ... ]
  u64 new_reference = memory->free;

  printf("    MovePair: moving from %llu in the_objects to %llu in new_objects\n", ref, new_reference);
  Object old_car = memory->the_objects[ref];
  if (IsBrokenHeart(old_car)) {
    // The pair has already been moved. Return the updated reference.
    // Old: [ ..., <BH new>, ... ]
    printf("    MovePair: old_car is a broken heart pointing to %llu\n", UnboxReference(old_car));
    return BoxPair(UnboxReference(old_car));
  }

  printf("    MovePair: Moving from the_objects to new_objects, leaving a broken heart at %llu pointing to %llu\n", ref, new_reference);
  Object the_pair = BoxPair(new_reference);
  // Old: [ ..., car, cdr, ... ]
  memory->new_objects[memory->free++] = old_car;
  memory->new_objects[memory->free++] = memory->the_objects[ref+1];
  memory->the_objects[ref] = BoxBrokenHeart(new_reference);
  // New: [ ..., car, cdr, free.. ]
  // Old: [ ...,  <BH new>, ... ]
  return the_pair;
}

// Returns ceiling(numerator/denominator)
u64 CeilingU64(u64 numerator, u64 denominator) {
  return (numerator + denominator - 1) / denominator;
}

u64 NumObjectsPerBlob(u64 bytes_in_blob) {
  // number of objects required to hold at least bytes_in_blob + 1 for the header
  return 1 + CeilingU64(bytes_in_blob, sizeof(Object));
}

u64 MoveBlob(struct Memory *memory, u64 ref) {
  // New: [ ..., free... ]
  // Old: [ ..., nBytes, byte0, ..., byteN, pad.., ] OR
  //      [ ..., <BH new>, ... ]
  u64 new_reference = memory->free;

  printf("    MoveBlob: moving from %llu in the_objects to %llu in new_objects\n", ref, new_reference);
  Object old_header = memory->the_objects[ref];
  if (IsBrokenHeart(old_header)) {
    // Already been moved
    // Old: [ ..., <BH new>, ... ]
    printf("    MoveBlob: old_header is a broken heart pointing to %llu\n", UnboxReference(old_header));
    return UnboxReference(old_header);
  }

  // Old: [ ..., nBytes, byte0, ..., byteN, pad.., ... ]
  assert(IsFixnum(old_header));
  u64 bytes_in_blob = UnboxFixnum(old_header);
  u64 num_objects = NumObjectsPerBlob(bytes_in_blob);
  printf("    MoveBlob: moving blob of size %llu bytes, (%llu objects)\n", bytes_in_blob, num_objects);

  memcpy(&memory->new_objects[memory->free], &memory->the_objects[ref], num_objects*sizeof(Object));
  memory->free += num_objects;
  // New: [ ..., nBytes, byte0, ..., byteN, pad.., free.. ]

  printf("    MoveBlob: Leaving a broken heart pointing at %llu in its place\n", new_reference);
  memory->the_objects[ref] = BoxBrokenHeart(new_reference);
  // Old: [ ..., <BH new>, ... ]

  return new_reference;
}

Object MoveString(struct Memory *memory, u64 ref)     { return BoxString(MoveBlob(memory, ref)); }
Object MoveSymbol(struct Memory *memory, u64 ref)     { return BoxSymbol(MoveBlob(memory, ref)); }
Object MoveByteVector(struct Memory *memory, u64 ref) { return BoxByteVector(MoveBlob(memory, ref)); }

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

struct Memory AllocateMemory(u64 max_objects) {
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
  memory.root = nil;
  return memory;
}

void EnsureEnoughMemory(struct Memory *memory, u64 num_objects_required) {
  if (!(memory->free + num_objects_required <= memory->max_objects)) {
    CollectGarbage(memory);
  }
  assert(memory->free + num_objects_required <= memory->max_objects);
}

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
u64 AllocateBlob(struct Memory *memory, u64 num_bytes) {
  u64 num_objects = NumObjectsPerBlob(num_bytes);
  EnsureEnoughMemory(memory, num_objects);
  // [ ..., free.. ]

  u64 new_reference = memory->free;
  memory->the_objects[new_reference] = BoxFixnum(num_bytes);
  memory->free += num_objects;
  memory->num_objects_allocated += num_objects;
  // [ ..., nBytes, byte0, ..., byteN, pad.., free.. ]

  return new_reference;
}

Object AllocateByteVector(struct Memory *memory, u64 num_bytes) {
  u64 new_reference = AllocateBlob(memory, num_bytes);
  memset(&memory->the_objects[new_reference + 1], 0, num_bytes);
  return BoxByteVector(new_reference);
}

Object ByteVectorRef(struct Memory *memory, u64 reference, u64 index) {
  assert(index < UnboxFixnum(memory->the_objects[reference]));
  u8 *bytes = (u8*)&memory->the_objects[reference+1];
  return BoxFixnum(bytes[index]);
}
void ByteVectorSet(struct Memory *memory, u64 reference, u64 index, u8 value) {
  assert(index < UnboxFixnum(memory->the_objects[reference]));
  u8 *bytes = (u8*)&memory->the_objects[reference+1];
  bytes[index] = value;
}

Object AllocateString(struct Memory *memory, const char *string) {
  u64 num_bytes = strlen(string) + 1;
  u64 new_reference = AllocateBlob(memory, num_bytes);
  memcpy(&memory->the_objects[new_reference + 1], string, num_bytes);
  return BoxString(new_reference);
}
Object AllocateSymbol(struct Memory *memory, const char *name) {
  return BoxSymbol(UnboxReference(AllocateString(memory, name)));
}

Object AllocatePair(struct Memory *memory, Object car, Object cdr) {
  EnsureEnoughMemory(memory, 2);
  // [ ..., free.. ]
  u64 new_reference = memory->free;
  memory->the_objects[memory->free++] = car;
  memory->the_objects[memory->free++] = cdr;
  memory->num_objects_allocated += 2;
  // [ ..., car, cdr, free.. ]
  return BoxPair(new_reference);
}
Object Car(struct Memory *memory, u64 reference) {
  return memory->the_objects[reference];
}
Object Cdr(struct Memory *memory, u64 reference) {
  return memory->the_objects[reference + 1];
}
void SetCar(struct Memory *memory, u64 reference, Object value) {
  memory->the_objects[reference] = value;
}
void SetCdr(struct Memory *memory, u64 reference, Object value) {
  memory->the_objects[reference + 1] = value;
}

void PrintObject(struct Memory *memory, Object object) {
  if (IsReal64(object)) { 
    printf("%llf", UnboxReal64(object));
    return;
  }
  switch (GetTag(object)) {
    case TAG_NIL:    printf("nil"); break;
    case TAG_TRUE:   printf("#t"); break;
    case TAG_FALSE:  printf("#f"); break;
    case TAG_FIXNUM: printf("%lld", UnboxFixnum(object)); break;
    case TAG_REAL32: printf("%ff", UnboxReal32(object)); break;

    // Reference Objects
    case TAG_PAIR: {
      u64 reference = UnboxReference(object);
      printf("(");
      PrintObject(memory, memory->the_objects[reference]);
      printf(" . ");
      PrintObject(memory, memory->the_objects[reference+1]);
      printf(")");
    } break;
    case TAG_STRING: {
      u64 reference = UnboxReference(object);
      printf("\"%s\"", (const char*)&memory->the_objects[reference+1]);
    } break;
    case TAG_SYMBOL: {
      u64 reference = UnboxReference(object);
      // TODO: Handle escaping pipe characters
      printf("%s", (const char*)&memory->the_objects[reference+1]);
    } break;
    case TAG_VECTOR: {
      u64 reference = UnboxReference(object);
      u64 length = UnboxFixnum(memory->the_objects[reference]);
      printf("(vector");
      for (u64 index = 0; index < length; ++index) {
        printf(" ");
        PrintObject(memory, memory->the_objects[reference+1 + index]);
      }
      printf(")");
    } break;
    case TAG_BYTE_VECTOR: {
      u64 reference = UnboxReference(object);
      printf("(byte-vector");
      s64 length = UnboxFixnum(memory->the_objects[reference]);
      u8 *bytes = (u8*)&memory->the_objects[reference+1];
      for (s64 index = 0; index < length; ++index) {
        printf(" 0x%x", bytes[index]);
      }
      printf(")");
    } break;
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
      case TAG_NIL:    printf("nil"); break;
      case TAG_TRUE:   printf("true"); break;
      case TAG_FALSE:  printf("false"); break;
      case TAG_FIXNUM: printf("%lld", UnboxFixnum(object)); break;
      case TAG_REAL32: printf("%ff", UnboxReal32(object)); break;
      // Reference Objects
      case TAG_PAIR: printf("<Pair %llu>", UnboxReference(object)); break;
      case TAG_STRING: printf("<String %llu>", UnboxReference(object)); break;
      case TAG_SYMBOL: printf("<Symbol %llu>", UnboxReference(object)); break;
      case TAG_VECTOR: printf("<Vector %llu>", UnboxReference(object)); break;
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
  struct Memory memory = AllocateMemory(32);
  AllocatePair(&memory, BoxFixnum(4), BoxFixnum(2));
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

  Object shared = AllocatePair(&memory, byte_vector, string);
  memory.root = AllocatePair(&memory, shared, AllocatePair(&memory, shared, vector));

  printf("Old Root: ");
  PrintlnObject(&memory, memory.root);
  PrintMemory(&memory);
  CollectGarbage(&memory);

  printf("New Root: ");
  PrintlnObject(&memory, memory.root);
  PrintMemory(&memory);

  for (int i = 0; i < 1000; ++i) {
    AllocatePair(&memory, BoxFixnum(0), BoxFixnum(1));
  }
  printf("Root: ");
  PrintlnObject(&memory, memory.root);
  printf("Allocated %llu objects, performed %llu garbage collections, moved %llu objects,\n"
      "on average: %llf objects allocated/collection, %llf objects moved/collection\n",
      memory.num_objects_allocated, memory.num_collections, memory.num_objects_moved,
      memory.num_objects_allocated * 1.0 / memory.num_collections,
      memory.num_objects_moved * 1.0 / memory.num_collections);

  memory.root = nil;
  memory.root = AllocateVector(&memory, 31);
  printf("Root: ");
  PrintlnObject(&memory, memory.root);

  memory.root = nil;
  // NOTE: out of memory error
  //memory.root = AllocateVector(&memory, 32);
}

