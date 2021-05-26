#ifndef ROOT_H
#define ROOT_H

#include "tag.h"

// At the root of memory is a vector of registers, holding all of the data/references
// needed for the program.

void InitializeRoot();

enum Register {
  REGISTER_SYMBOL_TABLE,
  // Needed for reading lists
  REGISTER_READ_STACK,

  REGISTER_EXPRESSION,
  REGISTER_VALUE,
  REGISTER_ENVIRONMENT,
  REGISTER_ARGUMENT_LIST,
  REGISTER_UNEVALUATED,
  NUM_REGISTERS,
};

Object GetRegister(enum Register reg);
void SetRegister(enum Register reg, Object value);

#endif
