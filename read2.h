#ifndef READ2_H
#define READ2_H

#include "c_types.h"
#include "error.h"

// Read an object from the string in REGISTER_READ_SOURCE, starting
// from REGISTER_READ_SOURCE_POSITION.
// The read-in object is left in REGISTER_EXPRESSION.
// The REGISTER_READ_SOURCE_POSITION is left pointing to the first unconsumed character.
// If an error occurs, the error code is set.
void ReadFromSource(enum ErrorCode *error);

// Copies source string into the REGISTER_READ_SOURCE.
void SetReadSourceFromString(const u8 *source, enum ErrorCode *error);

// Copies the file contents of filename into REGISTER_READ_SOURCE.
void SetReadSourceFromFile(const u8 *filename, enum ErrorCode *error);

void TestRead2();

#endif
