#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "error.h"
#include "tag.h"

// The Symbol Table is a Hashed set of Symbol Objects.
// Internally, it is a Vector Object whose elements are Lists of symbols.
// It is used to uniquely store references to symbols, so that they can be compared
// for equality by testing to see if the references are equal, instead of a deep equality check.

// Intialize the global symbol table.
void InitializeSymbolTable(u64 size, enum ErrorCode *error);

u32 HashString(const u8 *str);
Object MakeSymbolTable(u64 size, enum ErrorCode *error);

Object FindSymbol(const u8 *name);
Object InternSymbol(const u8 *name, enum ErrorCode *error);
void UninternSymbol(const u8 *name);

void TestSymbolTable();

#endif
