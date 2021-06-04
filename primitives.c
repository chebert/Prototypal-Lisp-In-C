#include "primitives.h"

#include "log.h"
#include "pair.h"

// Sets the error code and returns nil.
Object InvalidArgumentError(enum ErrorCode *error);
// Sets the error code and returns nil.
Object DivideByZeroError(enum ErrorCode *error);
// Checks for underflow or overflow and returns the boxed fixnum.
// If an error occurs, returns nil and sets the error code.
Object FixnumArithmeticResult(s64 result, enum ErrorCode *error);

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

// Assert that arguments is nil, otherwise set error and return nil.
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

Object InvalidArgumentError(enum ErrorCode *error) {
  *error = ERROR_EVALUATE_INVALID_ARGUMENT_TYPE;
  return nil;
}
Object DivideByZeroError(enum ErrorCode *error) {
  *error = ERROR_EVALUATE_DIVIDE_BY_ZERO;
  return nil;
}

Object FixnumArithmeticResult(s64 result, enum ErrorCode *error) {
  if (result < most_negative_fixnum) {
    *error = ERROR_EVALUATE_ARITHMETIC_UNDERFLOW;
    return nil;
  } else if (result > most_positive_fixnum) {
    *error = ERROR_EVALUATE_ARITHMETIC_OVERFLOW;
    return nil;
  } else {
    return BoxFixnum(result);
  }
}

void ExtractArgument(Object *arguments, Object *result, enum ErrorCode *error) {
  if (IsNil(*arguments)) {
    *error = ERROR_EVALUATE_ARITY_MISMATCH;
  } else {
    *result = First(*arguments);
    *arguments = Rest(*arguments);
  }
}

void CheckEmptyArguments(Object *arguments, enum ErrorCode *error) {
  if (!IsNil(*arguments)) {
    *error = ERROR_EVALUATE_ARITY_MISMATCH;
  }
}

void Extract2Arguments(Object *arguments, Object *a, Object *b, enum ErrorCode *error) {
  ExtractArgument(arguments, a, error);
  if (*error) return;
  ExtractArgument(arguments, b, error);
  if (*error) return;
  CheckEmptyArguments(arguments, error);
}

// Return the result of (Fixnum `operator` Real64Object)
#define PERFORM_BINARY_ARITHMETIC_FIXNUM_REAL(fixnum_value, b, operator, error) \
  do { \
    if (IsReal64(b)) return BoxReal64(fixnum_value operator UnboxReal64(b)); \
    return InvalidArgumentError(error); \
  } while (0)

// Perform binary arithmetic operation (fixnum operator b), assuming
// that IsFixnum(fixnum)
#define PERFORM_BINARY_ARITHMETIC_FIXNUM(fixnum, b, operator, error) \
  do { \
    s64 aval = UnboxFixnum(fixnum); \
    if (IsFixnum(b)) return FixnumArithmeticResult(aval operator UnboxFixnum(b), error); \
    PERFORM_BINARY_ARITHMETIC_FIXNUM_REAL(aval, b, operator, error); \
  } while (0)

// Perform binary arithmetic operation (real operator b), assuming
// that IsReal64(real)
#define PERFORM_BINARY_ARITHMETIC_REAL64(real, b, operator, error) \
  do { \
    real64 aval = UnboxReal64(a); \
    if (IsFixnum(b)) return BoxReal64(aval operator UnboxFixnum(b)); \
    if (IsReal64(b)) return BoxReal64(aval operator UnboxReal64(b)); \
    return InvalidArgumentError(error); \
  } while (0)

// Perform binary arithmetic operation (a operator b)
#define PERFORM_BINARY_ARITHMETIC(a, b, operator, error) \
  do { \
    if (IsFixnum(a)) PERFORM_BINARY_ARITHMETIC_FIXNUM(a, b, operator, error); \
    if (IsReal64(a)) PERFORM_BINARY_ARITHMETIC_REAL64(a, b, operator, error); \
    return InvalidArgumentError(error); \
  } while (0)

Object PrimitiveBinaryAdd(Object arguments, enum ErrorCode *error) {
  Object a, b;
  Extract2Arguments(&arguments, &a, &b, error);
  if (*error) return nil;
  PERFORM_BINARY_ARITHMETIC(a, b, +, error);
}

Object PrimitiveBinarySubtract(Object arguments, enum ErrorCode *error) {
  Object a, b;
  Extract2Arguments(&arguments, &a, &b, error);
  if (*error) return nil;
  PERFORM_BINARY_ARITHMETIC(a, b, -, error);
}

Object PrimitiveBinaryMultiply(Object arguments, enum ErrorCode *error) {
  Object a, b;
  Extract2Arguments(&arguments, &a, &b, error);
  if (*error) return nil;
  PERFORM_BINARY_ARITHMETIC(a, b, *, error);
}

Object PrimitiveBinaryDivide(Object arguments, enum ErrorCode *error) {
  Object a, b;
  Extract2Arguments(&arguments, &a, &b, error);
  if (*error) return nil;

  if (IsFixnum(a)) {
    s64 aval = UnboxFixnum(a);
    if (IsFixnum(b)) {
      s64 bval = UnboxFixnum(b);
      if (bval == 0)
        return DivideByZeroError(error);
      return BoxFixnum(aval / bval);
    } else {
      PERFORM_BINARY_ARITHMETIC_FIXNUM_REAL(aval, b, /, error);
    }
  } 
  if (IsReal64(a))
    PERFORM_BINARY_ARITHMETIC_REAL64(a, b, /, error);
  return InvalidArgumentError(error);
}

Object PrimitiveRemainder(Object arguments, enum ErrorCode *error) {
  Object a, b;
  Extract2Arguments(&arguments, &a, &b, error);
  if (*error) return nil;

  if (IsFixnum(a) && IsFixnum(b))
    return BoxFixnum(UnboxFixnum(a) % UnboxFixnum(b));
  return InvalidArgumentError(error);
}
