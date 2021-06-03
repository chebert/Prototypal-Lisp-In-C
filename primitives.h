#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include "error.h"
#include "tag.h"

#define PRIMITIVES \
  X("+:binary", PrimitiveBinaryAdd) \
  X("-:unary", PrimitiveUnarySubtract) \
  X("-:binary", PrimitiveBinarySubtract) \
  X("*:binary", PrimitiveBinaryMultiply) \
  X("/:binary", PrimitiveBinaryDivide) \
  X("remainder", PrimitiveRemainder)

#define X(str, primitive_name) Object primitive_name(Object arguments, enum ErrorCode *error);
  PRIMITIVES
#undef X

#endif
