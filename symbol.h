#ifndef SYMBOL_H
#define SYMBOL_H

#include "memory.h"

Object AllocateSymbol(struct Memory *memory, const char *name);

Object MoveSymbol(struct Memory *memory, u64 ref);
void PrintSymbol(struct Memory *memory, Object symbol);

#endif
