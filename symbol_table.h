#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "tag.h"

// The Symbol Table is a Hashed set of Symbol Objects.
// Internally, it is a Vector Object whose elements are Lists of symbols.
// It is used to uniquely store references to symbols, so that they can be compared
// for equality by testing to see if the references are equal, instead of a deep equality check.

// TODO: InitializeSymbolTable(u64 size);

u32 HashString(const u8 *str);
Object MakeSymbolTable(u64 size);

Object FindSymbol(const char *name);
Object InternSymbol(const char *name);
void UninternSymbol(const char *name);
void TestSymbolTable();

#endif
