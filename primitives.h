#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include "error.h"
#include "tag.h"

Object PrimitiveUnaryAdd(Object arguments, enum ErrorCode *error);
Object PrimitiveBinaryAdd(Object arguments, enum ErrorCode *error);
Object PrimitiveUnarySubtract(Object arguments, enum ErrorCode *error);
Object PrimitiveBinarySubtract(Object arguments, enum ErrorCode *error);

#define PRIMITIVES \
  X("+:unary", PrimitiveUnaryAdd) \
  X("+:binary", PrimitiveBinaryAdd) \
  X("-:unary", PrimitiveUnarySubtract) \
  X("-:binary", PrimitiveBinarySubtract)

#endif
