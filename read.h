#ifndef READ_H
#define READ_H

#include "c_types.h"
#include "error.h"

// Read an object from the string in REGISTER_READ_SOURCE, starting
// from position.
// The read-in object is left in REGISTER_EXPRESSION.
// The position is left pointing to the first unconsumed character.
// If an error occurs, the error code is set.
void ReadFromSource(s64 *position, enum ErrorCode *error);

// Copies source string into the REGISTER_READ_SOURCE.
void SetReadSourceFromString(const u8 *source, enum ErrorCode *error);

// Copies the file contents of filename into REGISTER_READ_SOURCE.
void SetReadSourceFromFile(const u8 *filename, enum ErrorCode *error);

void TestRead();

#endif
