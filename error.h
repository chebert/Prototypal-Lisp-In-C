#ifndef ERROR_H
#define ERROR_H

#include "c_types.h"

#define ERROR_CODES \
  X(NO_ERROR) \
  X(ERROR_READ_INVALID_INTEGER) \
  X(ERROR_READ_INVALID_REAL32) \
  X(ERROR_READ_INVALID_REAL64) \
  X(ERROR_READ_UNTERMINATED_STRING) \
  X(ERROR_READ_UNTERMINATED_LIST) \
  X(ERROR_READ_PAIR_SEPARATOR_IN_FIRST_POSITION) \
  X(ERROR_READ_INVALID_PAIR_SEPARATOR) \
  X(ERROR_READ_TOO_MANY_OBJECTS_IN_PAIR) \
  X(ERROR_READ_UNMATCHED_LIST_CLOSE) \
  X(ERROR_BYTE_VECTOR_LENGTH_NON_BYTE_VECTOR) \
  X(ERROR_BYTE_VECTOR_REFERENCE_NON_BYTE_VECTOR) \
  X(ERROR_BYTE_VECTOR_SET_NON_BYTE_VECTOR) \
  X(ERROR_INDEX_OUT_OF_RANGE) \
  X(ERROR_VECTOR_LENGTH_NON_VECTOR) \
  X(ERROR_VECTOR_REFERENCE_NON_VECTOR) \
  X(ERROR_VECTOR_SET_NON_VECTOR) \
  X(ERROR_EVALUATE_UNKNOWN_PROCEDURE_TYPE) \
  X(ERROR_EVALUATE_UNKNOWN_EXPRESSION) \
  X(ERROR_EVALUATE_UNBOUND_VARIABLE) \
  X(ERROR_EVALUATE_ARITY_MISMATCH) \
  X(ERROR_EVALUATE_INVALID_ARGUMENT_TYPE) \
  X(ERROR_EVALUATE_SET_UNBOUND_VARIABLE) \
  X(ERROR_EVALUATE_IF_TOO_MANY_ARGUMENTS) \
  X(ERROR_EVALUATE_IF_MALFORMED) \
  X(ERROR_EVALUATE_SET_TOO_MANY_ARGUMENTS) \
  X(ERROR_EVALUATE_SET_MALFORMED) \
  X(ERROR_EVALUATE_SET_NON_SYMBOL) \
  X(ERROR_EVALUATE_DEFINE_TOO_MANY_ARGUMENTS) \
  X(ERROR_EVALUATE_DEFINE_MALFORMED) \
  X(ERROR_EVALUATE_DEFINE_NON_SYMBOL) \
  X(ERROR_EVALUATE_QUOTE_TOO_MANY_ARGUMENTS) \
  X(ERROR_EVALUATE_QUOTE_MALFORMED) \
  X(ERROR_EVALUATE_LAMBDA_BODY_SHOULD_BE_NON_EMPTY) \
  X(ERROR_EVALUATE_LAMBDA_BODY_MALFORMED) \
  X(ERROR_EVALUATE_LAMBDA_BODY_SHOULD_BE_LIST) \
  X(ERROR_EVALUATE_LAMBDA_MALFORMED) \
  X(ERROR_EVALUATE_LAMBDA_PARAMETERS_SHOULD_BE_LIST) \
  X(ERROR_EVALUATE_APPLICATION_DOTTED_LIST) \
  X(ERROR_EVALUATE_BEGIN_EMPTY) \
  X(ERROR_EVALUATE_BEGIN_MALFORMED) \
  X(ERROR_EVALUATE_SEQUENCE_EMPTY) \
  X(ERROR_EVALUATE_DIVIDE_BY_ZERO) \
  X(ERROR_EVALUATE_ARITHMETIC_OVERFLOW) \
  X(ERROR_EVALUATE_ARITHMETIC_UNDERFLOW) \
  X(ERROR_COULD_NOT_OPEN_BINARY_FILE_FOR_READING) \
  X(ERROR_COULD_NOT_CLOSE_FILE) \
  X(ERROR_COULD_NOT_SEEK_TO_START_OF_FILE) \
  X(ERROR_COULD_NOT_SEEK_TO_END_OF_FILE) \
  X(ERROR_COULD_NOT_TELL_FILE_POSITION) \
  X(ERROR_COULD_NOT_ALLOCATE_HEAP) \
  X(ERROR_COULD_NOT_ALLOCATE_HEAP_BUFFER) \
  X(ERROR_OUT_OF_MEMORY)

enum ErrorCode {
#define X(value) value,
  ERROR_CODES
#undef X
  NUM_ERROR_CODES
};

const u8 *ErrorCodeString(enum ErrorCode error);

#endif
