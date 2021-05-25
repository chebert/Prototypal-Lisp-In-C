#ifndef READ_H
#define READ_H

#include "c_types.h"

// Read an object from the source string.
// If object was read successfully, success is marked true,
// and the object is left in the REGISTER_READ_RESULT.
// The remaining (unconsumed) source is returned, beginning
// immediately after the end of the last object.
// Whitespace and comments are consumed and discarded.
const u8 *ReadObject(const u8 *source, b64 *success);

// Tests reading objects into the buffer.
void TestRead();

#endif
