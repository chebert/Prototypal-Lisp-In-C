#include "primitives.h"

#include "pair.h"

// Remove one argument from arguments, and assigning it to result.
// If there are no more arguments in arguments, error is set, and nil is returned.
#define EXTRACT_ARGUMENT(arguments, result, error) \
  do { \
    if (IsNil(arguments)) { \
      *error = ERROR_EVALUATE_ARITY_MISMATCH; \
      return nil; \
    } \
    result = First(arguments); \
    arguments = Rest(arguments); \
  } while (0)

#define CHECK_NO_MORE_ARGUMENTS(arguments, error) \
  do { \
    if (!IsNil(arguments)) { \
      *error = ERROR_EVALUATE_ARITY_MISMATCH; \
      return nil; \
    } \
  } while (0)

// Performs the type test. If the type is invalid, the error is set and nil is returned.
#define CHECK_TYPE(test, error) \
  do { \
    if (!test) { \
      *error = ERROR_EVALUATE_INVALID_ARGUMENT_TYPE; \
      return nil; \
    } \
  } while (0)

Object PrimitiveBinaryAdd(Object arguments, enum ErrorCode *error) {
  Object a, b;
  EXTRACT_ARGUMENT(arguments, a, error);
  EXTRACT_ARGUMENT(arguments, b, error);
  CHECK_NO_MORE_ARGUMENTS(arguments, error);

  if (IsFixnum(a)) {
    s64 aval = UnboxFixnum(a);
    if      (IsFixnum(b)) return BoxFixnum(aval + UnboxFixnum(b));
    else if (IsReal64(b)) return BoxReal64(aval + UnboxReal64(b));
  } else if (IsReal64(a)) {
    real64 aval = UnboxReal64(a);
    if      (IsFixnum(b)) return BoxReal64(aval + UnboxFixnum(b));
    else if (IsReal64(b)) return BoxReal64(aval + UnboxReal64(b));
  }
  *error = ERROR_EVALUATE_INVALID_ARGUMENT_TYPE;
  return nil;
}

Object PrimitiveUnarySubtract(Object arguments, enum ErrorCode *error) {
  Object a;
  EXTRACT_ARGUMENT(arguments, a, error);
  CHECK_NO_MORE_ARGUMENTS(arguments, error);

  if (IsFixnum(a))
    return BoxFixnum(-UnboxFixnum(a));
  else if (IsReal64(a))
    return BoxReal64(-UnboxReal64(a));

  *error = ERROR_EVALUATE_INVALID_ARGUMENT_TYPE;
  return nil;
}

Object PrimitiveBinarySubtract(Object arguments, enum ErrorCode *error) {
  Object a, b;
  EXTRACT_ARGUMENT(arguments, a, error);
  EXTRACT_ARGUMENT(arguments, b, error);
  CHECK_NO_MORE_ARGUMENTS(arguments, error);

  if (IsFixnum(a)) {
    s64 aval = UnboxFixnum(a);
    if      (IsFixnum(b)) return BoxFixnum(aval - UnboxFixnum(b));
    else if (IsReal64(b)) return BoxReal64(aval - UnboxReal64(b));
  } else if (IsReal64(a)) {
    real64 aval = UnboxReal64(a);
    if      (IsFixnum(b)) return BoxReal64(aval - UnboxFixnum(b));
    else if (IsReal64(b)) return BoxReal64(aval - UnboxReal64(b));
  }
  *error = ERROR_EVALUATE_INVALID_ARGUMENT_TYPE;
  return nil;
}

Object PrimitiveBinaryMultiplication(Object arguments, enum ErrorCode *error) {
  Object a, b;
  EXTRACT_ARGUMENT(arguments, a, error);
  EXTRACT_ARGUMENT(arguments, b, error);
  CHECK_NO_MORE_ARGUMENTS(arguments, error);

  if (IsFixnum(a)) {
    s64 aval = UnboxFixnum(a);
    if      (IsFixnum(b)) return BoxFixnum(aval * UnboxFixnum(b));
    else if (IsReal64(b)) return BoxReal64(aval * UnboxReal64(b));
  } else if (IsReal64(a)) {
    real64 aval = UnboxReal64(a);
    if      (IsFixnum(b)) return BoxReal64(aval * UnboxFixnum(b));
    else if (IsReal64(b)) return BoxReal64(aval * UnboxReal64(b));
  }
  *error = ERROR_EVALUATE_INVALID_ARGUMENT_TYPE;
  return nil;
}

Object PrimitiveBinaryDivision(Object arguments, enum ErrorCode *error) {
  Object a, b;
  EXTRACT_ARGUMENT(arguments, a, error);
  EXTRACT_ARGUMENT(arguments, b, error);
  CHECK_NO_MORE_ARGUMENTS(arguments, error);

  if (IsFixnum(a)) {
    s64 aval = UnboxFixnum(a);
    if (IsFixnum(b)) {
      s64 bval = UnboxFixnum(b);
      if (bval != 0) {
        return BoxFixnum(aval * bval);
      } else {
        *error = ERROR_EVALUATE_DIVIDE_BY_ZERO;
        return nil;
      }
    }
    else if (IsReal64(b)) return BoxReal64(aval / UnboxReal64(b));
  } else if (IsReal64(a)) {
    real64 aval = UnboxReal64(a);
    if      (IsFixnum(b)) return BoxReal64(aval / UnboxFixnum(b));
    else if (IsReal64(b)) return BoxReal64(aval / UnboxReal64(b));
  }

  *error = ERROR_EVALUATE_INVALID_ARGUMENT_TYPE;
  return nil;
}
