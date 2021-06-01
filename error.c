#include "error.h"

#include <assert.h>

static const u8 *error_code_strings[] = {
#define X(value, string) string,
    ERROR_CODES
#undef X
  };

const u8 *ErrorCodeString(enum ErrorCode error) {
  assert(error < NUM_ERROR_CODES);
  return error_code_strings[error];
};
