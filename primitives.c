#include "primitives.h"

#include "log.h"
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
  LOG_ERROR("%lld, %lld", most_negative_fixnum, most_positive_fixnum);
  if (result < most_negative_fixnum) {
    *error = ERROR_EVALUATE_ARITHMETIC_UNDERFLOW;
    return nil;
  }
  if (result > most_positive_fixnum) {
    *error = ERROR_EVALUATE_ARITHMETIC_OVERFLOW;
    return nil;
  }
  return BoxFixnum(result);
}

#define EXTRACT_2_ARGUMENTS(a, b, arguments, error) \
  do { \
    EXTRACT_ARGUMENT(arguments, a, error); \
    EXTRACT_ARGUMENT(arguments, b, error); \
    CHECK_NO_MORE_ARGUMENTS(arguments, error); \
  } while (0)

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
  EXTRACT_2_ARGUMENTS(a, b, arguments, error);
  PERFORM_BINARY_ARITHMETIC(a, b, +, error);
}

Object PrimitiveBinarySubtract(Object arguments, enum ErrorCode *error) {
  Object a, b;
  EXTRACT_2_ARGUMENTS(a, b, arguments, error);
  PERFORM_BINARY_ARITHMETIC(a, b, -, error);
}

Object PrimitiveBinaryMultiply(Object arguments, enum ErrorCode *error) {
  Object a, b;
  EXTRACT_2_ARGUMENTS(a, b, arguments, error);
  PERFORM_BINARY_ARITHMETIC(a, b, *, error);
}

Object PrimitiveBinaryDivide(Object arguments, enum ErrorCode *error) {
  Object a, b;
  EXTRACT_2_ARGUMENTS(a, b, arguments, error);
  if (IsFixnum(a)) {
    s64 aval = UnboxFixnum(a);
    if (IsFixnum(b)) {
      s64 bval = UnboxFixnum(b);
      if (bval == 0)
        return DivideByZeroError(error);
      return BoxFixnum(aval / bval);
    } 

    PERFORM_BINARY_ARITHMETIC_FIXNUM_REAL(aval, b, /, error);
  } 
  if (IsReal64(a))
    PERFORM_BINARY_ARITHMETIC_REAL64(a, b, /, error);
  return InvalidArgumentError(error);
}

Object PrimitiveRemainder(Object arguments, enum ErrorCode *error) {
  Object a, b;
  EXTRACT_2_ARGUMENTS(a, b, arguments, error);
  if (IsFixnum(a) && IsFixnum(b))
    return BoxFixnum(UnboxFixnum(a) % UnboxFixnum(b));
  return InvalidArgumentError(error);
}
