#include "root.h"

#include "symbol_table.h"

enum Registers {
  REGISTER_SYMBOL_TABLE,
  REGISTER_SAVED_CAR,
  REGISTER_SAVED_CDR,
  REGISTER_THE_OBJECT,
  NUM_REGISTERS,
};

void InitializeRoot(struct Memory *memory, u64 symbol_table_size) {
  memory->root = AllocateVector(memory, NUM_REGISTERS);
  VectorSet(memory, UnboxReference(memory->root), REGISTER_SYMBOL_TABLE, MakeSymbolTable(memory, symbol_table_size));
}

Object GetSymbolTable(struct Memory *memory) {
  return VectorRef(memory, UnboxReference(memory->root), REGISTER_SYMBOL_TABLE);
}

void SavePair(struct Memory *memory, Object car, Object cdr) {
  VectorSet(memory, UnboxReference(memory->root), REGISTER_SAVED_CAR, car);
  VectorSet(memory, UnboxReference(memory->root), REGISTER_SAVED_CDR, cdr);
}

void RestoreSavedPair(struct Memory *memory, Object *car, Object *cdr) {
  *car = VectorRef(memory, UnboxReference(memory->root), REGISTER_SAVED_CAR);
  *cdr = VectorRef(memory, UnboxReference(memory->root), REGISTER_SAVED_CDR);
  SavePair(memory, nil, nil);
}

void SetTheObject(struct Memory *memory, Object object) {
  VectorSet(memory, UnboxReference(memory->root), REGISTER_THE_OBJECT, object);
}
Object GetTheObject(struct Memory *memory) {
  return VectorRef(memory, UnboxReference(memory->root), REGISTER_THE_OBJECT);
}
