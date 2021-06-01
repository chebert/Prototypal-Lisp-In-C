#ifndef STRING_H
#define STRING_H

#include "error.h"
#include "tag.h"

// A string is an array of UTF-8 characters, with a NULL or 0 terminator at the end.
// It is implemented as a Blob, whose size is string_length+1 to include the terminator.

Object AllocateString(const char *string, enum ErrorCode *error);
Object MoveString(Object string);
void PrintString(Object object);

// Returns a U8 pointer from the start of the character buffer,
// Valid only until the next garbage collection.
u8 *StringCharacterBuffer(Object string);

#endif
