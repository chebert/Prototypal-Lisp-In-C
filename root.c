#include "root.h"

#include "memory.h"
#include "symbol_table.h"
#include "vector.h"

void InitializeRoot() {
  memory.root = AllocateVector(NUM_REGISTERS);
}

Object GetRegister(enum Register reg) {
  return VectorRef(memory.root, reg); 
}

void SetRegister(enum Register reg, Object value) {
  VectorSet(memory.root, reg, value); 
}
