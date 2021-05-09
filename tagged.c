#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

typedef int64_t s64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint8_t u8;
typedef s64 b64;
typedef float real32;

typedef u64 Object;

// Type Tags for Objects
enum Tag {
  TAG_BROKEN_HEART,
  TAG_UNSIGNED_INTEGER,
  TAG_SIGNED_INTEGER,
  TAG_BOOLEAN,
  TAG_BYTE,
  TAG_REAL32,
  TAG_NIL,
  TAG_PAIR,
};

// Boxing/Unboxing Objects
Object BoxReal32(real32 value);

u64 UnboxUnsignedInteger(Object object);
s64 UnboxSignedInteger(Object object);
s64 UnboxBoolean(Object object);
u8 UnboxByte(Object object);
real32 UnboxReal32(Object object);
u64 UnboxPair(Object object);

// Type Queries
enum Tag GetTag(Object object);

b64 IsPointerToPair(Object object);
b64 IsBrokenHeart(Object object);

// TODO:
//  Consolidate the_cars/the_cdrs
//  Add vector
//  Add byte_vector/strings
//  Add short-generation and long-generation

struct Memory {
  Object *the_cars;
  Object *the_cdrs;
  Object *new_cars; 
  Object *new_cdrs;

  u64 free;
  u64 scan;
  Object root;
};

struct Memory AllocateMemory(u64 num_pairs);
void CollectGarbage(struct Memory *memory);
Object AllocatePair(struct Memory *memory, Object car, Object cdr);
Object Car(struct Memory *memory, Object pair);
Object Cdr(struct Memory *memory, Object pair);

#define SHIFTL(mask, amount) ((u64)(mask) << (amount))
#define SHIFTR(mask, amount) ((u64)(mask) >> (amount))

#define TAG_SHIFT 60
#define TAG_MASK  SHIFTL(0b1111, TAG_SHIFT)

#define UNTAG(object)    ((u64)object & ~TAG_MASK)
#define TAG(value, tag)  ((Object)(SHIFTL(tag, TAG_SHIFT) | UNTAG(value)))

static Object broken_heart = (Object)(TAG(0, TAG_BROKEN_HEART));

enum Tag GetTag(Object object) { return (TAG_MASK & object) >> TAG_SHIFT; }

b64 IsPointerToPair(Object object) { return GetTag(object) == TAG_PAIR; }
b64 IsBrokenHeart(Object object) { return GetTag(object) == TAG_BROKEN_HEART; }

u64 UnboxUnsignedInteger(Object object) {
  return (u64)(UNTAG(object));
}
s64 UnboxSignedInteger(Object object) {
  u64 val = (u64)object;
  if (SHIFTL(1, TAG_SHIFT-1) & val) {
    // If the highest bit is set, then sign-extend
    return (s64)(TAG_MASK | val);
  }
  return (s64)(UNTAG(object));
}
s64 UnboxBoolean(Object object) {
  return (s64)(UNTAG(object));
}
u8 UnboxByte(Object object) {
  return (u8)(0xff & (u64)object);
}
real32 UnboxReal32(Object object) {
  union {
    real32 real32;
    u32 u32;
  } u;
  u.u32 = (0xffffffff & (u64)object);
  return u.real32;
}
u64 UnboxPair(Object object) {
  return (u64)(UNTAG(object));
}

Object BoxReal32(real32 value) {
  union {
    real32 real32;
    u32 u32;
  } u;
  u.real32 = value;
  return TAG(u.u32, TAG_REAL32);
}

// Relocates old from the_cars/the_cdrs into new_cars/new_cdrs
// returned value is the new location
Object RelocateOldResultInNew(struct Memory *memory, Object old);

struct Memory AllocateMemory(u64 num_pairs) {
  struct Memory memory;
  memory.free = memory.scan = 0;
  memory.root = TAG(0, TAG_NIL);

  u64 array_size = num_pairs * sizeof(Object);
  memory.the_cars = (Object*)malloc(array_size);
  memory.the_cdrs = (Object*)malloc(array_size);
  memory.new_cars = (Object*)malloc(array_size);
  memory.new_cdrs = (Object*)malloc(array_size);
  return memory;
}

Object AllocatePair(struct Memory *memory, Object car, Object cdr) {
  u64 new_index = memory->free;

  // Allocate a new pair
  Object result = TAG(new_index, TAG_PAIR);
  ++(memory->free);

  memory->the_cars[new_index] = car;
  memory->the_cdrs[new_index] = cdr;

  return result;
}

Object Car(struct Memory *memory, Object pair) {
  enum Tag tag = GetTag(pair);
  if (tag == TAG_NIL) {
    return pair;
  } else if (tag == TAG_PAIR) {
    u64 index = UnboxPair(pair);
    return memory->the_cars[index];
  } else {
    assert(!"Car: Invalid object");
  }
}
Object Cdr(struct Memory *memory, Object pair) {
  enum Tag tag = GetTag(pair);
  if (tag == TAG_NIL) {
    return pair;
  } else if (tag == TAG_PAIR) {
    u64 index = UnboxPair(pair);
    return memory->the_cars[index];
  } else {
    assert(!"Cdr: Invalid object");
  }
}

void CollectGarbage(struct Memory *memory) {
  // Begin scanning at the beginning of allocated memory,
  // Begin free region at the beginning of new_cars/new_cdrs
  memory->free = memory->scan = 0;

  // Start by moving the root
  memory->root = RelocateOldResultInNew(memory, memory->root);

  // Garbage Collection Loop
  for (; memory->scan != memory->free; ++memory->scan) {
    // Relocate the car of the pair that just moved.
    // Update the car of the pair to point to the relocated location.
    memory->new_cars[memory->scan] = RelocateOldResultInNew(memory, memory->new_cars[memory->scan]);

    // Relocate the cdr of the pair that just moved.
    // Update the cdr of the pair that just moved.
    memory->new_cdrs[memory->scan] = RelocateOldResultInNew(memory, memory->new_cdrs[memory->scan]);
  }

  // Scan reached the end. Completed relocation.
  // GCFlip
  // Swap the old cars/cdrs with the new cars/cdrs
  Object *temp = memory->the_cdrs;
  memory->the_cdrs = memory->new_cdrs;
  memory->new_cdrs = temp;

  temp = memory->the_cars;
  memory->the_cars = memory->new_cars;
  memory->new_cars = temp;
}

Object RelocateOldResultInNew(struct Memory *memory, Object old) {
  // Input: old is the Object to relocate
  // Output:
  //    if it's a pair:
  //      the_cars/the_cdrs will have a broken heart
  //      new_cars/new_cdrs will have a pair allocated with the same car/cdr as the old pair
  //    new is the new Object
  if (IsPointerToPair(old)) {
    // The Object is a pair, copy the car and cdr over and leave a broken heart.

    u64 old_index = UnboxPair(old);
    Object old_car = memory->the_cars[old_index];
    if (IsBrokenHeart(old_car)) {
      // Already been moved.
      return memory->the_cdrs[old_index];
    } else {
      // Move the pair to the next free location.
      u64 new_index = memory->free;

      // Allocate a new pair
      Object new = TAG(new_index, TAG_PAIR);
      ++(memory->free);

      // Copy the old car
      memory->new_cars[new_index] = old_car;
      // Copy the old cdr.
      memory->new_cdrs[new_index] = memory->the_cdrs[old_index];

      // Leave a broken heart, which points at the new memory address.
      memory->the_cars[old_index] = broken_heart;
      memory->the_cdrs[old_index] = new;
      return new;
    }
  } else {
    // The object is a primitive and can be reassigned directly
    return old;
  }
}

s64 TwosComplement(u64 value) {
  return (s64)(~value + 1);
}

int main(int argc, char** argv) {
  assert(TAG_MASK == 0xf000000000000000);
  assert(~TAG_MASK == 0x0fffffffffffffff);
  assert(UnboxUnsignedInteger(TAG(0xfeeddeadbeefcafe, TAG_UNSIGNED_INTEGER)) == 0x0eeddeadbeefcafe);

  assert(-1 == UnboxSignedInteger(TAG(-1, TAG_SIGNED_INTEGER)));
  assert(TwosComplement(SHIFTL(1, TAG_SHIFT-1)) == UnboxSignedInteger(TAG(SHIFTL(1, TAG_SHIFT-1), TAG_SIGNED_INTEGER)) ? "good" : "bad");

  assert(UnboxSignedInteger(TAG(SHIFTL(1, TAG_SHIFT-1) - 1, TAG_SIGNED_INTEGER)) == 0x07ffffffffffffff);

  assert(UnboxBoolean(TAG(1, TAG_BOOLEAN)));
  assert(!UnboxBoolean(TAG(0, TAG_BOOLEAN)));

  assert(UnboxByte(TAG(0xBE, TAG_BYTE)) == 0xBE);
  assert(UnboxPair(TAG(42, TAG_PAIR)) == 42);
  
  assert(GetTag(TAG(0xBE, TAG_BYTE)) == TAG_BYTE);
  assert(GetTag(TAG(42, TAG_PAIR)) == TAG_PAIR);
  assert(GetTag(TAG(0xFEEDDEADBEEFCAFE, TAG_UNSIGNED_INTEGER)) == TAG_UNSIGNED_INTEGER);
  assert(GetTag(TAG(1, TAG_BOOLEAN)) == TAG_BOOLEAN);

  assert(UnboxReal32(BoxReal32(3.14159f)) == 3.14159f);
  assert(GetTag(BoxReal32(3.14159f)) == TAG_REAL32);
}
