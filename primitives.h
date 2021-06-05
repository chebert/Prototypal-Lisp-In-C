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
  X("remainder", PrimitiveRemainder) \
\
  X("allocate-byte-vector", PrimitiveAllocateByteVector) \
  X("byte-vector?", PrimitiveIsByteVector) \
  X("byte-vector-length", PrimitiveByteVectorLength) \
  X("byte-vector-set!", PrimitiveByteVectorSet) \
  X("byte-vector-ref", PrimitiveByteVectorRef) \
  X("string->byte-vector", PrimitiveStringToByteVector) \
  X("byte-vector->string", PrimitiveByteVectorToString) \
\
  X("symbol->string", PrimitiveSymbolToString) \
  X("intern", PrimitiveIntern) \
  X("unintern", PrimitiveUnintern) \
  X("find-symbol", PrimitiveFindSymbol) \
\
  X("allocate-vector", PrimitiveAllocateVector) \
  X("vector?", PrimitiveIsVector) \
  X("vector-length", PrimitiveVectorLength) \
  X("vector-set!", PrimitiveVectorSet) \
  X("vector-ref", PrimitiveVectorRef) \
\
  X("allocate-pair", PrimitiveAllocatePair) \
  X("list", PrimitiveList) \
  X("pair?", PrimitiveIsPair) \
  X("pair-left", PrimitivePairLeft) \
  X("pair-right", PrimitivePairRight) \
  X("set-pair-left!", PrimitiveSetPairLeft) \
  X("set-pair-right!", PrimitiveSetPairRight) \
\
  X("eq?", PrimitiveEq) \
  X("evaluate", PrimitiveEvaluate) \
\
  X("open-binary-file-for-reading!", PrimitiveOpenBinaryFileForReading) \
  X("file-length", PrimitiveFileLength) \
  X("copy-file-contents!", PrimitiveCopyFileContents) \
  X("close-file!", PrimitiveCloseFile)

// TODO: vector, byte-vector

#define X(str, primitive_name) Object primitive_name(Object arguments, enum ErrorCode *error);
  PRIMITIVES
#undef X

#endif
