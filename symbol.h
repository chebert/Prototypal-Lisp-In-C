#ifndef SYMBOL_H
#define SYMBOL_H

#include "error.h"
#include "tag.h"

// A Symbol is a string with a different tag to indicate that it is a symbol.
// Symbols should all be part of the symbol table, and therefore can be compared
// by reference instead of doing a deep comparison.

Object AllocateSymbol(const char *name, enum ErrorCode *error);

Object MoveSymbol(Object symbol);
void PrintSymbol(Object symbol);

#endif
