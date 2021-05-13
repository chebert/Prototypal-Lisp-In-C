#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "tag.h"
#include "memory.h"

u32 HashString(const u8 *str);
Object MakeSymbolTable(struct Memory *memory, u64 size);

Object FindSymbol(struct Memory *memory, Object symbol_table, const char *name);
Object InternSymbol(struct Memory *memory, Object symbol_table, const char *name);
void UninternSymbol(struct Memory *memory, Object symbol_table, const char *name);
void TestSymbolTable();

#endif
