#include "primitives.h"

#include <stdio.h>

#include "evaluate.h"
#include "log.h"
#include "pair.h"
#include "read.h"
#include "root.h"
#include "byte_vector.h"
#include "string.h"
#include "symbol_table.h"
#include "vector.h"

// TODO: Change primitives to continuation passing style?

// Checks the error code and returns nil if there is an error.
#define CHECK(error) do { if (*error) return nil; } while (0)

#define DECLARE_PRIMITIVE(name, arguments_name, error_name)  \
  Object name(Object arguments_name, enum ErrorCode *error_name)

// Sets the error code and returns nil.
Object InvalidArgumentError(enum ErrorCode *error);
// Sets the error code and returns nil.
Object DivideByZeroError(enum ErrorCode *error);
// Checks for underflow or overflow and returns the boxed fixnum.
// If an error occurs, returns nil and sets the error code.
Object FixnumArithmeticResult(s64 result, enum ErrorCode *error);

// Extract one argument from the arguments list. Sets the error code if no arguments left.
void ExtractArgument(Object *arguments, Object *result, enum ErrorCode *error);
// Extract one argument from the arguments list. Sets the error code if there is not exactly one argument.
void Extract1Argument(Object *arguments, Object *a, enum ErrorCode *error);
// Extracts two arguments from the arguments list. Sets the error code if there is not exactly two arguments.
void Extract2Arguments(Object *arguments, Object *a, Object *b, enum ErrorCode *error);
// Extracts three arguments from the arguments list. Sets the error code if there is not exactly three arguments.
void Extract3Arguments(Object *arguments, Object *a, Object *b, Object *c, enum ErrorCode *error);
// Sets the error code if arguments is not empty.
void CheckEmptyArguments(Object arguments, enum ErrorCode *error);

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

void CheckEmptyArguments(Object arguments, enum ErrorCode *error) {
  if (!IsNil(arguments)) {
    *error = ERROR_EVALUATE_ARITY_MISMATCH;
  }
}

void Extract1Argument(Object *arguments, Object *a, enum ErrorCode *error) {
  ExtractArgument(arguments, a, error);
  if (*error) return;
  CheckEmptyArguments(*arguments, error);
}

void Extract2Arguments(Object *arguments, Object *a, Object *b, enum ErrorCode *error) {
  ExtractArgument(arguments, a, error);
  if (*error) return;
  ExtractArgument(arguments, b, error);
  if (*error) return;
  CheckEmptyArguments(*arguments, error);
}

void Extract3Arguments(Object *arguments, Object *a, Object *b, Object *c, enum ErrorCode *error) {
  ExtractArgument(arguments, a, error);
  if (*error) return;
  ExtractArgument(arguments, b, error);
  if (*error) return;
  ExtractArgument(arguments, c, error);
  if (*error) return;
  CheckEmptyArguments(*arguments, error);
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

#define BINARY_ARITHMETIC_DEFINITION(operator, arguments, error) \
  do { \
    Object a, b; \
    Extract2Arguments(&arguments, &a, &b, error); \
    CHECK(error); \
    PERFORM_BINARY_ARITHMETIC(a, b, operator, error); \
  } while (0)


DECLARE_PRIMITIVE(PrimitiveUnarySubtract, arguments, error) {
  Object a;
  Extract1Argument(&arguments, &a, error);
  CHECK(error);

  if (IsFixnum(a))
    return BoxFixnum(-UnboxFixnum(a));
  else if (IsReal64(a))
    return BoxReal64(-UnboxReal64(a));

  *error = ERROR_EVALUATE_INVALID_ARGUMENT_TYPE;
  return nil;
}

DECLARE_PRIMITIVE(PrimitiveBinaryAdd, arguments, error) {
  BINARY_ARITHMETIC_DEFINITION(+, arguments, error);
}

DECLARE_PRIMITIVE(PrimitiveBinarySubtract, arguments, error) {
  BINARY_ARITHMETIC_DEFINITION(-, arguments, error);
}

DECLARE_PRIMITIVE(PrimitiveBinaryMultiply, arguments, error) {
  BINARY_ARITHMETIC_DEFINITION(*, arguments, error);
}

DECLARE_PRIMITIVE(PrimitiveBinaryDivide, arguments, error) {
  Object a, b;
  Extract2Arguments(&arguments, &a, &b, error);
  CHECK(error);

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

DECLARE_PRIMITIVE(PrimitiveRemainder, arguments, error) {
  Object a, b;
  Extract2Arguments(&arguments, &a, &b, error);
  CHECK(error);

  if (IsFixnum(a) && IsFixnum(b))
    return BoxFixnum(UnboxFixnum(a) % UnboxFixnum(b));
  return InvalidArgumentError(error);
}

DECLARE_PRIMITIVE(PrimitiveAllocatePair, arguments, error) {
  CheckEmptyArguments(arguments, error);
  CHECK(error);

  return AllocatePair(error);
}
DECLARE_PRIMITIVE(PrimitiveIsPair, arguments, error) {
  Object object;
  Extract1Argument(&arguments, &object, error);
  CHECK(error);
  return BoxBoolean(IsPair(object));
}
DECLARE_PRIMITIVE(PrimitivePairLeft, arguments, error) {
  Object pair;
  Extract1Argument(&arguments, &pair, error);
  CHECK(error);
  if (!IsPair(pair)) return InvalidArgumentError(error);
  return Car(pair);
}
DECLARE_PRIMITIVE(PrimitivePairRight, arguments, error) {
  Object pair;
  Extract1Argument(&arguments, &pair, error);
  CHECK(error);
  if (!IsPair(pair)) return InvalidArgumentError(error);
  return Cdr(pair);
}
DECLARE_PRIMITIVE(PrimitiveSetPairLeft, arguments, error) {
  Object pair, value;
  Extract2Arguments(&arguments, &pair, &value, error);
  CHECK(error);
  if (!IsPair(pair)) return InvalidArgumentError(error);
  SetCar(pair, value);
  return FindSymbol("ok");
}
DECLARE_PRIMITIVE(PrimitiveSetPairRight, arguments, error) {
  Object pair, value;
  Extract2Arguments(&arguments, &pair, &value, error);
  CHECK(error);
  if (!IsPair(pair)) return InvalidArgumentError(error);
  SetCdr(pair, value);
  return FindSymbol("ok");
}
DECLARE_PRIMITIVE(PrimitiveList, arguments, error) { return arguments; }

DECLARE_PRIMITIVE(PrimitiveEq, arguments, error) {
  Object a, b;
  Extract2Arguments(&arguments, &a, &b, error);
  CHECK(error);
  return BoxBoolean(a == b);
}

DECLARE_PRIMITIVE(PrimitiveEvaluate, arguments, error) {
  Object expression;
  Extract1Argument(&arguments, &expression, error);
  CHECK(error);
  return Evaluate(expression);
}

DECLARE_PRIMITIVE(PrimitiveStringToByteVector, arguments, error) {
  Object string;
  Extract1Argument(&arguments, &string, error);
  CHECK(error);
  if (!IsString(string)) return InvalidArgumentError(error);
  return BoxByteVector(UnboxReference(string));
}
DECLARE_PRIMITIVE(PrimitiveByteVectorToString, arguments, error) {
  Object byte_vector;
  Extract1Argument(&arguments, &byte_vector, error);
  CHECK(error);
  if (!IsByteVector(byte_vector)) return InvalidArgumentError(error);
  return BoxString(UnboxReference(byte_vector));
}

DECLARE_PRIMITIVE(PrimitiveOpenBinaryFileForReading, arguments, error) {
  Object string;
  Extract1Argument(&arguments, &string, error);
  CHECK(error);
  if (!IsString(string)) return InvalidArgumentError(error);
  FILE *file = fopen(StringCharacterBuffer(string), "rb");
  if (!file) {
    *error = ERROR_COULD_NOT_OPEN_BINARY_FILE_FOR_READING;
    // TODO: read errno?
    return nil;
  }
  return BoxFilePointer(file);
}

DECLARE_PRIMITIVE(PrimitiveFileLength, arguments, error) {
  Object file;
  Extract1Argument(&arguments, &file, error);
  CHECK(error);
  if (!IsFilePointer(file)) return InvalidArgumentError(error);

  FILE *f = UnboxFilePointer(file);
  if (fseek(f, 0, SEEK_END)) {
    *error = ERROR_COULD_NOT_SEEK_TO_END_OF_FILE;
    return nil;
  }
  s64 length = ftell(f);
  if (length < 0) {
    *error = ERROR_COULD_NOT_TELL_FILE_POSITION;
    return nil;
  }
  return BoxFixnum(length);
}

DECLARE_PRIMITIVE(PrimitiveCopyFileContents, arguments, error) {
  Object file, byte_vector;
  Extract2Arguments(&arguments, &file, &byte_vector, error);
  CHECK(error);
  if (!IsFilePointer(file)) return InvalidArgumentError(error);
  if (!IsByteVector(byte_vector)) return InvalidArgumentError(error);

  FILE *f = UnboxFilePointer(file);
  if (fseek(f, 0, SEEK_SET)) {
    *error = ERROR_COULD_NOT_SEEK_TO_START_OF_FILE;
    return nil;
  }
  s64 length = UnsafeByteVectorLength(byte_vector) - 1;
  // TODO: ByteVectorBuffer
  s64 num_bytes_read = fread(StringCharacterBuffer(byte_vector), 1, length, f);
  if (ferror(f)) {
    *error = ERROR_COULD_NOT_READ_FILE;
  }
  // Null-terminate
  UnsafeByteVectorSet(byte_vector, length, 0);

  return FindSymbol("ok");
}

DECLARE_PRIMITIVE(PrimitiveCloseFile, arguments, error) {
  Object file;
  Extract1Argument(&arguments, &file, error);
  CHECK(error);
  if (!IsFilePointer(file)) return InvalidArgumentError(error);

  FILE *f = UnboxFilePointer(file);
  if (fclose(f)) {
    *error = ERROR_COULD_NOT_CLOSE_FILE;
    return nil;
  }
  return FindSymbol("ok");
}

DECLARE_PRIMITIVE(PrimitiveAllocateByteVector, arguments, error) {
  Object num_bytes;
  Extract1Argument(&arguments, &num_bytes, error);
  CHECK(error);
  if (!IsFixnum(num_bytes)) return InvalidArgumentError(error);

  return AllocateByteVector(UnboxFixnum(num_bytes), error);
}

DECLARE_PRIMITIVE(PrimitiveIsByteVector, arguments, error) {
  Object byte_vector;
  Extract1Argument(&arguments, &byte_vector, error);
  CHECK(error);
  return BoxBoolean(IsByteVector(byte_vector));
}

DECLARE_PRIMITIVE(PrimitiveByteVectorLength, arguments, error) {
  Object byte_vector;
  Extract1Argument(&arguments, &byte_vector, error);
  CHECK(error);
  if (!IsByteVector(byte_vector)) return InvalidArgumentError(error);
  return BoxFixnum(UnsafeByteVectorLength(byte_vector));
}

DECLARE_PRIMITIVE(PrimitiveByteVectorSet, arguments, error) {
  Object byte_vector, index, value;
  Extract3Arguments(&arguments, &byte_vector, &index, &value, error);
  CHECK(error);
  if (!IsByteVector(byte_vector)) return InvalidArgumentError(error);
  if (!IsFixnum(index)) return InvalidArgumentError(error);

  s64 length = UnsafeByteVectorLength(byte_vector);
  s64 index_value = UnboxFixnum(index);
  if (index_value < 0 || index_value >= length) {
    *error = ERROR_INDEX_OUT_OF_RANGE;
    return nil;
  }

  UnsafeByteVectorSet(byte_vector, index_value, value);
  return FindSymbol("ok");
}

DECLARE_PRIMITIVE(PrimitiveByteVectorRef, arguments, error) {
  Object byte_vector, index;
  Extract2Arguments(&arguments, &byte_vector, &index, error);
  CHECK(error);
  if (!IsByteVector(byte_vector)) return InvalidArgumentError(error);
  if (!IsFixnum(index)) return InvalidArgumentError(error);

  s64 length = UnsafeByteVectorLength(byte_vector);
  s64 index_value = UnboxFixnum(index);
  if (index_value < 0 || index_value >= length) {
    *error = ERROR_INDEX_OUT_OF_RANGE;
    return nil;
  }

  return UnsafeByteVectorRef(byte_vector, UnboxFixnum(index));
}

DECLARE_PRIMITIVE(PrimitiveSymbolToString, arguments, error) {
  Object symbol;
  Extract1Argument(&arguments, &symbol, error);
  CHECK(error);
  if (!IsSymbol(symbol)) return InvalidArgumentError(error);
  return BoxString(UnboxReference(symbol));
}

DECLARE_PRIMITIVE(PrimitiveIntern, arguments, error) {
  Object string;
  Extract1Argument(&arguments, &string, error);
  CHECK(error);
  if (!IsString(string)) return InvalidArgumentError(error);
  return InternSymbol(StringCharacterBuffer(string), error);
}

DECLARE_PRIMITIVE(PrimitiveUnintern, arguments, error) {
  Object string;
  Extract1Argument(&arguments, &string, error);
  CHECK(error);
  if (!IsString(string)) return InvalidArgumentError(error);
  UninternSymbol(StringCharacterBuffer(string));
  return FindSymbol("ok");
}

DECLARE_PRIMITIVE(PrimitiveFindSymbol, arguments, error) {
  Object string;
  Extract1Argument(&arguments, &string, error);
  CHECK(error);
  if (!IsString(string)) return InvalidArgumentError(error);
  return FindSymbol(StringCharacterBuffer(string));
}

DECLARE_PRIMITIVE(PrimitiveAllocateVector, arguments, error) {
  Object num_objects;
  Extract1Argument(&arguments, &num_objects, error);
  CHECK(error);
  if (!IsFixnum(num_objects)) return InvalidArgumentError(error);

  return AllocateVector(UnboxFixnum(num_objects), error);
}

DECLARE_PRIMITIVE(PrimitiveIsVector, arguments, error) {
  Object vector;
  Extract1Argument(&arguments, &vector, error);
  CHECK(error);
  return BoxBoolean(IsVector(vector));
}

DECLARE_PRIMITIVE(PrimitiveVectorLength, arguments, error) {
  Object vector;
  Extract1Argument(&arguments, &vector, error);
  CHECK(error);
  if (!IsVector(vector)) return InvalidArgumentError(error);
  return BoxFixnum(UnsafeVectorLength(vector));
}

DECLARE_PRIMITIVE(PrimitiveVectorSet, arguments, error) {
  Object vector, index, value;
  Extract3Arguments(&arguments, &vector, &index, &value, error);
  CHECK(error);
  if (!IsVector(vector)) return InvalidArgumentError(error);
  if (!IsFixnum(index)) return InvalidArgumentError(error);

  s64 length = UnsafeVectorLength(vector);
  s64 index_value = UnboxFixnum(index);
  if (index_value < 0 || index_value >= length) {
    *error = ERROR_INDEX_OUT_OF_RANGE;
    return nil;
  }
  UnsafeVectorSet(vector, index_value, value);
  return FindSymbol("ok");
}

DECLARE_PRIMITIVE(PrimitiveVectorRef, arguments, error) {
  Object vector, index;
  Extract2Arguments(&arguments, &vector, &index, error);
  CHECK(error);
  if (!IsVector(vector)) return InvalidArgumentError(error);
  if (!IsFixnum(index)) return InvalidArgumentError(error);

  s64 length = UnsafeVectorLength(vector);
  s64 index_value = UnboxFixnum(index);
  if (index_value < 0 || index_value >= length) {
    *error = ERROR_INDEX_OUT_OF_RANGE;
    return nil;
  }

  return UnsafeVectorRef(vector, UnboxFixnum(index));
}

DECLARE_PRIMITIVE(PrimitiveReadFromString, arguments, error) {
  Object string, position, continuation;
  Extract3Arguments(&arguments, &string, &position, &continuation, error);
  CHECK(error);
  if (!IsString(string)) return InvalidArgumentError(error);
  if (!IsFixnum(position)) return InvalidArgumentError(error);
  if (!IsProcedure(continuation)) return InvalidArgumentError(error);

  s64 position_s64 = UnboxFixnum(position);
  SetRegister(REGISTER_PRIMITIVE_A, continuation);
  Object expression = ReadFromString(string, &position_s64, error);
  SetRegister(PRIMITIVE_B, expression);

  SetRegister(PRIMITIVE_C, AllocatePair(error));
  SetCar(pair, BoxFixnum(position_s64));
}
