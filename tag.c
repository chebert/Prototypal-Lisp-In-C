#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>

#include "tag.h"

// Real64 Encodings
// Normal Encoding:
//  s | eee eeee eeee | fff...f
// 63              52  51     0
//
// s := sign bit
// e := exponent
// f := fraction
//
// Special encodings
//  s | 111 1111 1111 | n | uuu...u
// 63              52  51  50     0
//
// s := sign bit
// n := NAN if set, else INFINITY
// u := unused
//
// Unused encoding: -NAN
//  1 | 111 1111 1111 | 1 | uuu...u
// 63              52  51  50     0
//
// Techincally this is a -NAN, but this is a redundant encoding of NAN, and no double operations return it.
// Treating this as an invalid real64 encoding allows us to use the bottom 51 bits as an Object encoding.


// Object Encoding:
// xF     F    F    8      0   ...    0
//  1 111 1111 1111 1  ttt t   ddd ...dddd
// 63              51     47  46         0
//  |  Metadata                | Payload |
//
// tttt    = Tag id (0-15)
// ddd...d = Tag-dependent data

#define TAGGED_OBJECT_MASK (0xFFF8000000000000)

#define SHIFT_LEFT(mask, amount) ((u64)(mask) << (amount))
#define SHIFT_RIGHT(mask, amount) ((u64)(mask) >> (amount))

#define TAG_SHIFT 47
#define TAG_MASK  SHIFT_LEFT(0b1111, TAG_SHIFT)

b64 IsTagged(Object obj) { return (obj & TAGGED_OBJECT_MASK) == TAGGED_OBJECT_MASK; }
b64 IsReal64(Object obj) { return !IsTagged(obj); }

// METADATA is the initial 1s followed by the tag
#define METADATA_MASK (TAGGED_OBJECT_MASK | TAG_MASK)
// Payload is just the data (no tag)
#define PAYLOAD_MASK  (~METADATA_MASK)

enum Tag GetTag(Object object) { return SHIFT_RIGHT(TAG_MASK & object, TAG_SHIFT); }

// Functions to convert reals to bytes without casting.
real32 U32ToReal32(u32 value) {
  union { real32 real32; u32 u32; } u;
  u.u32 = value; 
  return u.real32;
}
u32 Real32ToU32(real32 value) {
  union { real32 real32; u32 u32; } u;
  u.real32 = value; 
  return u.u32;
}
real64 U64ToReal64(u64 value) {
  union { real64 real64; u64 u64; } u;
  u.u64 = value; 
  return u.real64;
}
u64 Real64ToU64(real64 value) {
  union { real64 real64; u64 u64; } u;
  u.real64 = value; 
  return u.u64;
}


b64 HasTag(Object object, enum Tag tag) {
  return IsTagged(object) && GetTag(object) == tag;
}

b64 IsBrokenHeart(Object object) { return HasTag(object, TAG_BROKEN_HEART); }
b64 IsBlobHeader(Object object)  { return HasTag(object, TAG_BLOB_HEADER); }
b64 IsFixnum(Object object)      { return HasTag(object, TAG_FIXNUM); }
b64 IsTrue(Object object)        { return HasTag(object, TAG_TRUE); }
b64 IsFalse(Object object)       { return HasTag(object, TAG_FALSE); }
b64 IsReal32(Object object)      { return HasTag(object, TAG_REAL32); }
b64 IsNil(Object object)         { return HasTag(object, TAG_NIL); }
b64 IsBoolean(Object object) {
  if (IsTagged(object)) {
    enum Tag tag = GetTag(object);
    return tag == TAG_TRUE || tag == TAG_FALSE;
  }
  return 0;
}

b64 IsPair(Object object)        { return HasTag(object, TAG_PAIR); }
b64 IsVector(Object object)      { return HasTag(object, TAG_VECTOR); }
b64 IsByteVector(Object object)  { return HasTag(object, TAG_BYTE_VECTOR); }
b64 IsString(Object object)      { return HasTag(object, TAG_STRING); }
b64 IsSymbol(Object object)      { return HasTag(object, TAG_SYMBOL); }

Object TagPayload(u64 payload, enum Tag tag) {
  return TAGGED_OBJECT_MASK | SHIFT_LEFT(tag, TAG_SHIFT) | payload;
}

Object nil   = TAGGED_OBJECT_MASK | SHIFT_LEFT(TAG_NIL,   TAG_SHIFT);
Object true  = TAGGED_OBJECT_MASK | SHIFT_LEFT(TAG_TRUE,  TAG_SHIFT);
Object false = TAGGED_OBJECT_MASK | SHIFT_LEFT(TAG_FALSE, TAG_SHIFT);

// TODO: Maintain sign when truncating?
Object BoxFixnum(s64 fixnum)        { return TagPayload(PAYLOAD_MASK & fixnum, TAG_FIXNUM); }
Object BoxBoolean(b64 boolean)      { return TagPayload(0, boolean ? TAG_TRUE : TAG_FALSE); }
Object BoxReal32(real32 value)      { return TagPayload(Real32ToU32(value),    TAG_REAL32); }
Object BoxPair(u64 reference)       { return TagPayload(PAYLOAD_MASK & reference, TAG_PAIR); }
Object BoxVector(u64 reference)     { return TagPayload(PAYLOAD_MASK & reference, TAG_VECTOR); }
Object BoxByteVector(u64 reference) { return TagPayload(PAYLOAD_MASK & reference, TAG_BYTE_VECTOR); }
Object BoxString(u64 reference)     { return TagPayload(PAYLOAD_MASK & reference, TAG_STRING); }
Object BoxSymbol(u64 reference)     { return TagPayload(PAYLOAD_MASK & reference, TAG_SYMBOL); }

Object BoxReal64(real64 value)       { return Real64ToU64(value); }
Object BoxBrokenHeart(u64 reference) { return TagPayload(reference, TAG_BROKEN_HEART); }
Object BoxBlobHeader(u64 num_bytes)  { return TagPayload(num_bytes, TAG_BLOB_HEADER); }

s64 UnboxFixnum(Object object) {
  u64 sign_bit_mask = SHIFT_LEFT(1, TAG_SHIFT-1); 
  b64 is_sign_bit_set = (sign_bit_mask & object) != 0; 

  // Sign-extend the highest bit
  return is_sign_bit_set
    ? (s64)(METADATA_MASK | object)
    : (s64)(PAYLOAD_MASK & object);
}

b64    UnboxBoolean(Object object)    { return IsFalse(object) ? 0 : 1; }
real32 UnboxReal32(Object object)     { return U32ToReal32((u32)(0xffffffff & object)); }
real64 UnboxReal64(Object object)     { return U64ToReal64(object); }
u64    UnboxReference(Object object)  { return (PAYLOAD_MASK & object); }
u64    UnboxBlobHeader(Object object) { return (PAYLOAD_MASK & object); }

// Just for testing.
s64 TwosComplement(u64 value) { return (s64)(~value + 1); }

void TestTag() {
  assert(NUM_TAGS < 16);

  assert(-1 == UnboxFixnum(BoxFixnum(-1)));
  assert(               SHIFT_LEFT(1, TAG_SHIFT-1) - 1 == UnboxFixnum(BoxFixnum(SHIFT_LEFT(1, TAG_SHIFT-1) - 1)));
  assert(TwosComplement(SHIFT_LEFT(1, TAG_SHIFT-1))    == UnboxFixnum(BoxFixnum(SHIFT_LEFT(1, TAG_SHIFT-1))));

  assert( UnboxBoolean(BoxBoolean(1 == 1)));
  assert(!UnboxBoolean(BoxBoolean(1 != 1)));

  assert(IsPair(BoxPair(42)));
  assert(IsBoolean(BoxBoolean(1)));

  assert(!IsBoolean(BoxReal32(3.14159f)));

  assert(UnboxReal32(BoxReal32(3.14159f)) == 3.14159f);
  assert(IsReal32(BoxReal32(3.14159f)));

  assert(UnboxReal64(BoxReal64(3.14159)) == 3.14159);
  assert(IsReal64(BoxReal64(3.14159)));
  assert(IsReal64(BoxReal64(NAN)));
  assert(IsReal64(BoxReal64(INFINITY)));
}
