#ifndef SYMBOL_H
#define SYMBOL_H

#include "memory.h"

// A Symbol is a string with a different tag to indicate that it is a symbol.
// Symbols should all be part of the symbol table, and therefore can be compared
// by reference instead of doing a deep comparison.

Object AllocateSymbol(struct Memory *memory, const char *name);

Object MoveSymbol(struct Memory *memory, u64 ref);
void PrintSymbol(struct Memory *memory, Object symbol);

#endif
