#include "rope.h"

#include "pair.h"

Object MakeRope(struct Memory *memory, s64 string_size) {
  Object rope = AllocatePair(memory);
  SetCar(memory, UnboxReference(rope), BoxFixnum(string_size));
  SetCdr(memory, UnboxReference(rope), nil);
  return rope;
}

Object AppendRope(struct Memory *memory, Object rope, u8 character) {
}

Object FinalizeRope(struct Memory *memory, Object rope) {
}

void TestRope() {
}
