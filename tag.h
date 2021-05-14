#ifndef TAG_H
#define TAG_H
#include <stdint.h>

// A tagged Object is a 64-bit structure with a tag and a payload.
// In this case, a technique called NAN-boxing is used. This means that
// an Object is either a double, or a 51-bit tag-and-payload structure.
// If the double represents a negative NAN (an essentially unused value),
// then we assume it must be a tagged type.
//
// To Box a value, we convert from a C-type to a tagged Object.
// To Unbox a value, we convert from a tagged Object to a C-type.
//
// The supported tags are listed in the Tag enumeration.

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
  TAG_NIL,   // Represent the end of a list, or no value
  TAG_TRUE,  // Represent the boolean value true
  TAG_FALSE, // Represent the boolean value false

  // Primitive Types
  TAG_FIXNUM, // Fixnum is a 47-bit signed integer
  TAG_REAL32, // 32-bit float

  // Reference Types (Payloads are indices into memory vectors)
  TAG_PAIR, // Pair consists of two Objects
  TAG_VECTOR, // Vector consists of a length N, followed by N objects
  TAG_BYTE_VECTOR, // Byte vector consists of a length N, followed by at least N bytes
  TAG_STRING, // String consists of a length N, followed by at least N+1 bytes. String is 0-terminated
  TAG_SYMBOL, // Symbol is a string with a different tag.

  // GC Types (Types not directly accessible)
  TAG_BROKEN_HEART, // Used to annotate referenced objects that have been moved during garbage collection.

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
b64 IsReal32(Object object);
b64 IsNil(Object object);
b64 IsBoolean(Object object); 
b64 IsPair(Object object);
b64 IsVector(Object object);
b64 IsByteVector(Object object);
b64 IsString(Object object);
b64 IsSymbol(Object object);

// Construct boxed values given native C types
Object BoxFixnum(s64 fixnum); // Truncates to 47 bits
Object BoxBoolean(b64 boolean);
Object BoxReal32(real32 value);
Object BoxReal64(real64 value);
// Construct referential data structures. References are indices.
Object BoxPair(u64 reference);
Object BoxVector(u64 reference);
Object BoxByteVector(u64 reference);
Object BoxString(u64 reference);
Object BoxSymbol(u64 reference);
// Box GC Types
Object BoxBrokenHeart(u64 reference);

extern Object nil;
extern Object true;
extern Object false;

// Unbox from Object to native C types
s64    UnboxFixnum(Object object); 
b64    UnboxBoolean(Object object);
real32 UnboxReal32(Object object);
real64 UnboxReal64(Object object);
// Unbox Pair, Vector, Byte Vector, String, Symbol
u64 UnboxReference(Object object);
// Unbox GC Types
u64 UnboxBrokenHeart(Object object);

// Function to run through tag tests.
void TestTag();
#endif
