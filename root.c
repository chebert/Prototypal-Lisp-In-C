#include "root.h"

#include "memory.h"
#include "symbol_table.h"
#include "vector.h"

void InitializeRoot(u64 symbol_table_size) {
  memory.root = AllocateVector(NUM_REGISTERS);
  VectorSet(memory.root, REGISTER_SYMBOL_TABLE, MakeSymbolTable(symbol_table_size));
}

Object GetRegister(enum Register reg) {
  return VectorRef(memory.root, reg); 
}

void SetRegister(enum Register reg, Object value) {
  VectorSet(memory.root, reg, value); 
}

void SavePair(Object car, Object cdr) {
  SetRegister(REGISTER_SAVED_CAR, car);
  SetRegister(REGISTER_SAVED_CDR, cdr);
}

void RestorePair(Object *car, Object *cdr) {
  *car = GetRegister(REGISTER_SAVED_CAR);
  *cdr = GetRegister(REGISTER_SAVED_CDR);
}
