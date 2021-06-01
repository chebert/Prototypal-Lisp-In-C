#ifndef ERROR_H
#define ERROR_H

#include "c_types.h"

#define ERROR_CODES \
  X(NO_ERROR, "ok") \
  X(ERROR_BYTE_VECTOR_LENGTH_NON_BYTE_VECTOR, "Attempt to retrieve byte vector length of an Object that isn't a byte-vector") \
  X(ERROR_BYTE_VECTOR_REFERENCE_NON_BYTE_VECTOR, "Attempt to reference a byte of an Object that isn't a byte-vector") \
  X(ERROR_BYTE_VECTOR_REFERENCE_INDEX_OUT_OF_RANGE, "Attempt to reference an out-of-range byte in a byte-vector") \
  X(ERROR_BYTE_VECTOR_SET_NON_BYTE_VECTOR, "Attempt to set a the byte of an Object that isn't a byte-vector") \
  X(ERROR_BYTE_VECTOR_SET_INDEX_OUT_OF_RANGE, "Attempt to set an out-of-range byte of an Object") \
  X(ERROR_OUT_OF_MEMORY, "Ran out of memory")

enum ErrorCode {
#define X(value, string) value,
  ERROR_CODES
#undef X
  NUM_ERROR_CODES
};

const u8 *ErrorCodeString(enum ErrorCode error);

#endif
