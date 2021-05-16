#ifndef READ_BUFFER_H
#define READ_BUFFER_H

#include "tag.h"

// Reset/initialize the read buffer.
// Buffer size indicates how big the initial buffer should be,
// as well as how much the buffer should grow by when the end is reached.
// Recommended value: 256 (or some multiple of 8).
void ResetReadBuffer(s64 buffer_size);

// Append a character to the end of the read buffer.
void AppendReadBuffer(u8 character);

// Return a single string with all of the characters in the read buffer copied into it.
Object FinalizeReadBuffer();

void TestReadBuffer();

#endif
