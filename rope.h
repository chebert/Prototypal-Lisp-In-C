#ifndef ROPE_H
#define ROPE_H

#include "memory.h"

// TODO: where to put the rope, so I still have access to it after allocating.

// A rope can chain together strings, to make dynamically sized strings.
// A rope can be finalized into a single string.

// Construct a rope of strings.
// Each string is string_size+1 in length.
// rope := (string-size . strings-list)
Object MakeRope(struct Memory *memory, s64 string_size);

// Append a character to the end of the rope.
Object AppendRope(struct Memory *memory, Object rope, u8 character);

// Return a single string with all of the characters in rope copied into it.
Object FinalizeRope(struct Memory *memory, Object rope);

void TestRope();

#endif
