#ifndef READ_H
#define READ_H

#include "c_types.h"
#include "error.h"
#include "tag.h"

// Read an object from the string, starting from position.
// The read-in object is returned.
// The position is left pointing to the first unconsumed character.
// Modifies: REGISTER_READ_RESULT, REGISTER_READ_STACK, and REGISTER_READ_SOURCE
// If an error occurs, the error code is set.
Object ReadFromString(Object string, s64 *position, enum ErrorCode *return_error);

// Copies source string into the REGISTER_READ_SOURCE.
void SetReadSourceFromString(const u8 *source, enum ErrorCode *error);

// Copies the file contents of filename into REGISTER_READ_SOURCE.
void SetReadSourceFromFile(const u8 *filename, enum ErrorCode *error);

void TestRead();

#endif
