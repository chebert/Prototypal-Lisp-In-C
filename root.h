#ifndef ROOT_H
#define ROOT_H

#include "tag.h"

// At the root of memory is a vector of registers, holding all of the data/references
// needed for the program.

void InitializeRoot(u64 symbol_table_size);

enum Register {
  REGISTER_SYMBOL_TABLE,
  REGISTER_THE_OBJECT,
  REGISTER_SAVED_CAR,
  REGISTER_SAVED_CDR,
  REGISTER_ROPE_SAVE,
  NUM_REGISTERS,
};

Object GetRegister(enum Register reg);
void SetRegister(enum Register reg, Object value);

void SavePair(Object car, Object cdr);
void RestorePair(Object *car, Object *cdr);

#endif
