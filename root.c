#include "root.h"

#include <assert.h>

#include "memory.h"
#include "pair.h"
#include "symbol_table.h"
#include "vector.h"

void InitializeRoot() {
  enum ErrorCode error = NO_ERROR;
  memory.root = AllocateVector(NUM_REGISTERS, &error);
  assert(!error);
}

Object GetRegister(enum Register reg) {
  return UnsafeVectorRef(memory.root, reg); 
}

void SetRegister(enum Register reg, Object value) {
  UnsafeVectorSet(memory.root, reg, value); 
}

void Save(enum Register reg, enum ErrorCode *error) {
  Object stack = AllocatePair(error);
  if (*error) return;
  SetCar(stack, GetRegister(reg));
  SetCdr(stack, GetRegister(REGISTER_STACK));
  SetRegister(REGISTER_STACK, stack);
}
void Restore(enum Register reg) {
  Object value = Car(GetRegister(REGISTER_STACK));
  SetRegister(reg, value);
  SetRegister(REGISTER_STACK, Cdr(GetRegister(REGISTER_STACK)));
}

EvaluateFunction GetContinue() {
  return UnboxEvaluateFunction(GetRegister(REGISTER_CONTINUE));
}
void SetContinue(EvaluateFunction func) {
  SetRegister(REGISTER_CONTINUE, BoxEvaluateFunction(func));
}

Object GetValue() { return GetRegister(REGISTER_VALUE); }
void SetValue(Object o) { SetRegister(REGISTER_VALUE, o); }

Object GetExpression() { return GetRegister(REGISTER_EXPRESSION); }
void SetExpression(Object o) { SetRegister(REGISTER_EXPRESSION, o); }

Object GetEnvironment() { return GetRegister(REGISTER_ENVIRONMENT); }
void SetEnvironment(Object o) { SetRegister(REGISTER_ENVIRONMENT, o); }

Object GetUnevaluated() { return GetRegister(REGISTER_UNEVALUATED); }
void SetUnevaluated(Object o) { SetRegister(REGISTER_UNEVALUATED, o); }

Object GetProcedure() { return GetRegister(REGISTER_PROCEDURE); }
void SetProcedure(Object o) { SetRegister(REGISTER_PROCEDURE, o); }

Object GetArgumentList() { return GetRegister(REGISTER_ARGUMENT_LIST); }
void SetArgumentList(Object o) { SetRegister(REGISTER_ARGUMENT_LIST, o); }
