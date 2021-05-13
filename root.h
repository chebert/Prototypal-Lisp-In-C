#ifndef ROOT_H
#define ROOT_H

#include "memory.h"

void InitializeRoot(struct Memory *memory, u64 symbol_table_size);
Object GetSymbolTable(struct Memory *memory);

void SavePair(struct Memory *memory, Object car, Object cdr);
void RestoreSavedPair(struct Memory *memory, Object *car, Object *cdr);

void SetTheObject(struct Memory *memory, Object object);
Object GetTheObject(struct Memory *memory);

#endif
