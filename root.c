#include "root.h"

#include "symbol_table.h"
#include "vector.h"

void InitializeRoot(struct Memory *memory, u64 symbol_table_size) {
  memory->root = AllocateVector(memory, NUM_REGISTERS);
  VectorSet(memory, UnboxReference(memory->root), REGISTER_SYMBOL_TABLE, MakeSymbolTable(memory, symbol_table_size));
}

Object GetRegister(struct Memory *memory, enum Register reg) {
  return VectorRef(memory, UnboxReference(memory->root), reg); 
}

void SetRegister(struct Memory *memory, enum Register reg, Object value) {
  VectorSet(memory, UnboxReference(memory->root), reg, value); 
}

void SavePair(struct Memory *memory, Object car, Object cdr) {
  SetRegister(memory, REGISTER_SAVED_CAR, car);
  SetRegister(memory, REGISTER_SAVED_CDR, cdr);
}

void RestorePair(struct Memory *memory, Object *car, Object *cdr) {
  *car = GetRegister(memory, REGISTER_SAVED_CAR);
  *cdr = GetRegister(memory, REGISTER_SAVED_CDR);
}
