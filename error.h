#ifndef ERROR_H
#define ERROR_H

#include "c_types.h"

#define ERROR_CODES \
  X(NO_ERROR) \
  X(ERROR_BYTE_VECTOR_LENGTH_NON_BYTE_VECTOR) \
  X(ERROR_BYTE_VECTOR_REFERENCE_NON_BYTE_VECTOR) \
  X(ERROR_BYTE_VECTOR_REFERENCE_INDEX_OUT_OF_RANGE) \
  X(ERROR_BYTE_VECTOR_SET_NON_BYTE_VECTOR) \
  X(ERROR_BYTE_VECTOR_SET_INDEX_OUT_OF_RANGE) \
  X(ERROR_VECTOR_LENGTH_NON_VECTOR) \
  X(ERROR_VECTOR_REFERENCE_NON_VECTOR) \
  X(ERROR_VECTOR_REFERENCE_INDEX_OUT_OF_RANGE) \
  X(ERROR_VECTOR_SET_NON_VECTOR) \
  X(ERROR_VECTOR_SET_INDEX_OUT_OF_RANGE) \
  X(ERROR_OUT_OF_MEMORY)

enum ErrorCode {
#define X(value) value,
  ERROR_CODES
#undef X
  NUM_ERROR_CODES
};

const u8 *ErrorCodeString(enum ErrorCode error);

#endif
