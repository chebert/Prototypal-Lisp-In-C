#ifndef ROPE_H
#define ROPE_H

#include "memory.h"

// A rope is a data structure which chains strings together, one after the other.
// When the rope is unable to append a character, the rope is extended to have a new, empty string.
// This enables the reading of unpredictably large strings/symbols,
// without wasting space.
//
// Internally a rope is a list of strings.
// A rope can be finalized into a single string.

// Construct a rope of strings.
// rope := (string-size . strings-list)
Object MakeRope(struct Memory *memory, s64 string_size);

// Append a character to the end of the rope.
Object AppendRope(struct Memory *memory, Object rope, u8 character);

// Return a single string with all of the characters in rope copied into it.
Object FinalizeRope(struct Memory *memory, Object rope);

void TestRope();

#endif
