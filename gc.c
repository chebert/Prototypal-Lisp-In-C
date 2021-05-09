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
  if (IsTagged(pair)) {
    enum Tag tag = GetTag(pair);
    if (IsNil(pair)) {
      return pair;
    } else if (tag == TAG_PAIR) {
      u64 index = UnboxPair(pair);
      return memory->the_cars[index];
    } else {
      assert(!"Car: Invalid object");
    }
  } else {
    assert(!"Car: Invalid object");
  }
}
Object Cdr(struct Memory *memory, Object pair) {
  if (IsTagged(pair)) {
    enum Tag tag = GetTag(pair);
    if (IsNil(pair)) {
      return pair;
    } else if (tag == TAG_PAIR) {
      u64 index = UnboxPair(pair);
      return memory->the_cdrs[index];
    } else {
      assert(!"Cdr: Invalid object");
    }
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
  if (IsPair(old)) {
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

