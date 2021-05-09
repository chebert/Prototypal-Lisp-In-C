#include <stdint.h>

// Simpler type names
typedef int64_t  s64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint8_t  u8;
typedef s64      b64;
typedef float    real32;
typedef double   real64;

// Objects are u64 so that we can easily perform bit manipulations.
typedef u64      Object;

// Type Tags for Objects
enum Tag {
  // Tag-only types (no payload)
  TAG_BROKEN_HEART,
  TAG_NIL,
  TAG_TRUE,
  TAG_FALSE,

  // Primitive Types
  TAG_FIXNUM,
  TAG_BYTE,
  TAG_REAL32,

  // Reference Types (Payloads are indices into memory vectors)
  TAG_PAIR,        // Memory: [ car, cdr ]
  TAG_VECTOR,      // Memory: [ nObjects, obj0, obj1 ... ]
  TAG_BYTE_VECTOR, // Memory: [ nBytes, byte0, byte1, ... ]
  TAG_STRING,      // Memory: [ nBytes, byte0, byte1, ... ]
  TAG_BIGNUM,      // Memory: ???

  NUM_TAGS,
};

// Objects are either a double, or a tagged value.
b64 IsReal64(Object obj);
b64 IsTagged(Object obj);

// Tags can be retrieved from tagged objects.
enum Tag GetTag(Object object);

// Helper functions determine if an Object is tagged, and whether
// it has the given tag.
b64 IsBrokenHeart(Object object);
b64 IsFixnum(Object object);
b64 IsTrue(Object object);
b64 IsFalse(Object object);
b64 IsByte(Object object);
b64 IsReal32(Object object);
b64 IsNil(Object object);
b64 IsBoolean(Object object); 
b64 IsPair(Object object);
b64 IsVector(Object object);
b64 IsByteVector(Object object);
b64 IsString(Object object);
b64 IsBignum(Object object);

// Construct boxed values given native C types
Object BoxFixnum(s64 fixnum); // Truncates to 47 bits
Object BoxBoolean(b64 boolean);
Object BoxByte(u8 byte);
Object BoxReal32(real32 value);
Object BoxReal64(real64 value);
// Construct referential data structures. References are indices.
Object BoxPair(u64 reference);
Object BoxVector(u64 reference);
Object BoxByteVector(u64 reference);
Object BoxString(u64 reference);
Object BoxBignum(u64 reference);

// Unbox from Object to native C types
s64    UnboxFixnum(Object object); 
b64    UnboxBoolean(Object object);
u8     UnboxByte(Object object);
real32 UnboxReal32(Object object);
real64 UnboxReal64(Object object);
// Unbox Pair, Vector, Byte Vector, String, and Bignum
u64 UnboxReference (Object object);
