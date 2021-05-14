#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "tag.h"
#include "memory.h"

// The Symbol Table is a Hashed set of Symbol Objects.
// Internally, it is a Vector Object whose elements are Lists of symbols.
// It is used to uniquely store references to symbols, so that they can be compared
// for equality by testing to see if the references are equal, instead of a deep equality check.

u32 HashString(const u8 *str);
Object MakeSymbolTable(struct Memory *memory, u64 size);

Object FindSymbol(struct Memory *memory, const char *name);
Object InternSymbol(struct Memory *memory, const char *name);
void UninternSymbol(struct Memory *memory, const char *name);
void TestSymbolTable();

#endif
