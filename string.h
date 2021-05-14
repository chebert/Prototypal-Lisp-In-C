#ifndef STRING_H
#define STRING_H

#include "memory.h"

// A string is an array of UTF-8 characters, with a NULL or 0 terminator at the end.
// It is implemented as a Blob, whose size is string_length+1 to include the terminator.

Object AllocateString(struct Memory *memory, const char *string);
Object MoveString(struct Memory *memory, u64 ref);
void PrintString(struct Memory *memory, Object object);

#endif
