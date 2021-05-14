#ifndef ROOT_H
#define ROOT_H

#include "memory.h"

// At the root of memory is a vector of registers, holding all of the data/references
// needed for the program.

void InitializeRoot(struct Memory *memory, u64 symbol_table_size);

enum Register {
  REGISTER_SYMBOL_TABLE,
  REGISTER_THE_OBJECT,
  REGISTER_SAVED_CAR,
  REGISTER_SAVED_CDR,
  NUM_REGISTERS,
};

Object GetRegister(struct Memory *memory, enum Register reg);
void SetRegister(struct Memory *memory, enum Register reg, Object value);

void SavePair(struct Memory *memory, Object car, Object cdr);
void RestorePair(struct Memory *memory, Object *car, Object *cdr);

#endif
