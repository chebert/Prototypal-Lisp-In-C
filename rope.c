#include "rope.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "byte_vector.h"
#include "memory.h"
#include "pair.h"
#include "root.h"
#include "string.h"

// The read buffer is implemented as a Rope.

// A rope is a data structure which chains strings together.
// When the rope is unable to append a character, the rope is extended to have a new, empty string.
// This enables the reading of unpredictably large strings/symbols/numbers,
// minimizing wasted space.
// A rope can be finalized into a single string.
//
// Internally a rope is a list of strands held in reverse order.
// A strand is a dynamically-extendable byte vector, whose current size is the strand_length.

// Rope := (max_strand_size . strands...)
// Strand := (strand_length . byte_vector)

// Strand constructor
Object AllocateStrand(s64 max_strand_size);
// Strand accessors
s64 StrandLength(Object strand);
Object StrandCharacters(Object strand);

u64 ListLength(Object list);

// True if another strand needs to be allocated to append a character to rope.
b64 IsRopeFull(Object rope);
// True if the length of the strand is equal to the max_strand_size of the Rope.
b64 IsStrandFull(Object strand, s64 max_strand_size);

// Append a character to the end of strand
void AppendCharacterToStrand(Object strand, u8 character);

Object MakeRope(s64 string_size);
// Return the max size of each string in rope.
s64 RopeMaxStringSize(Object rope);
// Return list of strands in rope.
Object RopeStrands(Object rope);
void SetRopeStrands(Object rope, Object strands);

u64 RopeLengthInBytes(Object rope);
void AppendNewStrand(Object rope);

void ResetReadBuffer(s64 buffer_size) {
  SetRegister(REGISTER_READ_BUFFER, MakeRope(buffer_size));
}

void AppendReadBuffer(u8 character) {
  Object rope = GetRegister(REGISTER_READ_BUFFER);
  if (IsRopeFull(rope)) {
    // The newest strand is full, allocate a new strand.
    AppendNewStrand(rope);
    // REFERENCES INVALIDATED
    rope = GetRegister(REGISTER_READ_BUFFER);
  }

  Object strands = RopeStrands(rope);
  Object newest_strand = First(strands);
  AppendCharacterToStrand(newest_strand, character);
}

Object FinalizeReadBuffer() {
  Object rope = GetRegister(REGISTER_READ_BUFFER);
  Object strands = RopeStrands(rope);
  if (strands == nil)
    return AllocateString("");

  u64 num_bytes = RopeLengthInBytes(rope);

  Object characters = AllocateByteVector(num_bytes);
  // REFERENCES INVALIDATED
  rope = GetRegister(REGISTER_READ_BUFFER);
  strands = RopeStrands(rope);

  u8 *ptr = StringCharacterBuffer(characters);
  u64 start_index = num_bytes-1; // TODO: or -2?
  // Copy over characters from each strand.
  for (; strands != nil; strands = Rest(strands)) {
    Object strand = First(strands);
    s64 length = StrandLength(strand);
    start_index -= length;
    Object strand_characters = StrandCharacters(strand);
    memcpy(ptr + start_index, StringCharacterBuffer(strand_characters), length);
  }

  // Null terminate the string
  ByteVectorSet(characters, num_bytes-1, 0);
  return BoxString(characters);
}

// strand := (num_bytes . string)
Object AllocateStrand(s64 max_strand_size) {
  Object characters = AllocateByteVector(max_strand_size);
  return MakePair(BoxFixnum(0), characters);
}

s64 StrandLength(Object strand) { return UnboxFixnum(Car(strand)); }
void SetStrandLength(Object strand, s64 length) { SetCar(strand, BoxFixnum(length)); }
Object StrandCharacters(Object strand) { return Cdr(strand); }

void AppendNewStrand(Object rope) {
  s64 max_strand_size = RopeMaxStringSize(rope);
  // The newest strand is full, allocate a new strand.
  Object strand = AllocateStrand(max_strand_size);
  // REFERENCES INVALIDATED
  rope = GetRegister(REGISTER_READ_BUFFER);
  Object strands = RopeStrands(rope);
  Object new_strands = MakePair(strand, strands);
  // REFERENCES INVALIDATEDS
  rope = GetRegister(REGISTER_READ_BUFFER);
  SetRopeStrands(rope, new_strands);
}

void AppendCharacterToStrand(Object strand, u8 character) {
  // Assumes there is room for at least 1 more character.
  s64 length = StrandLength(strand);
  Object characters = StrandCharacters(strand);

  // Copy the character, and increment the length.
  ByteVectorSet(characters, length, character);
  SetStrandLength(strand, length + 1);
}

b64 IsRopeFull(Object rope) {
  Object strands = RopeStrands(rope);
  return strands == nil || IsStrandFull(First(strands), RopeMaxStringSize(rope));
}
b64 IsStrandFull(Object strand, s64 max_strand_size) {
  return max_strand_size == StrandLength(strand);
}

Object MakeRope(s64 max_strand_size) {
  return MakePair(BoxFixnum(max_strand_size), nil);
}

s64 RopeMaxStringSize(Object rope) {
  return UnboxFixnum(Car(rope));
}
Object RopeStrands(Object rope) {
  return Cdr(rope);
}
void SetRopeStrands(Object rope, Object strands) {
  return SetCdr(rope, strands);
}

u64 ListLength(Object list) {
  u64 result = 0;
  for (; list != nil; list = Rest(list), ++result);
  return result;
}

u64 RopeLengthInBytes(Object rope) {
  s64 max_strand_size = RopeMaxStringSize(rope);
  Object strands = RopeStrands(rope);
  u64 num_strands = ListLength(strands);

  // All except the newest strand are guaranteed to be full (max_strand_size)
  u64 num_characters = (num_strands-1) * max_strand_size + StrandLength(First(strands));
  return num_characters + 1; // \0-terminator
}

void TestRope() {
  // Test appending values and garbage collecting in the midst of moving values.
  InitializeMemory(28);
  MakePair(BoxFixnum(1), BoxFixnum(2));
  MakePair(BoxFixnum(1), BoxFixnum(2));
  MakePair(BoxFixnum(1), BoxFixnum(2));
  MakePair(BoxFixnum(1), BoxFixnum(2));
  MakePair(BoxFixnum(1), BoxFixnum(2));
  ResetReadBuffer(2);
  printf("appending h\n");
  AppendReadBuffer('h');
  printf("appending e\n");
  AppendReadBuffer('e');
  printf("appending l\n");
  AppendReadBuffer('l');
  printf("appending l\n");
  AppendReadBuffer('l');
  printf("appending o\n");
  AppendReadBuffer('o');

  printf("finalizing\n");
  assert(!strcmp("hello", StringCharacterBuffer(FinalizeReadBuffer())));
  DestroyMemory();
}
