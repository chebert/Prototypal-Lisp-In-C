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
  TAG_NIL,
  TAG_TRUE,
  TAG_FALSE,

  // Primitive Types
  TAG_FIXNUM,
  TAG_REAL32,

  // Reference Types (Payloads are indices into memory vectors)
  TAG_PAIR,
  TAG_VECTOR,
  TAG_BYTE_VECTOR,
  TAG_STRING,

  // GC Types (Types not directly accessible)
  TAG_BROKEN_HEART,

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
// Unbox Pair, Vector, Byte Vector, String
u64 UnboxReference(Object object);
// Unbox GC Types
u64 UnboxBrokenHeart(Object object);

// Function to run through tag tests.
void TestTagged();
