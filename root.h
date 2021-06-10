#ifndef ROOT_H
#define ROOT_H

#include "error.h"
#include "tag.h"

// At the root of memory is a vector of registers, holding all of the data/references
// needed for the program.

void InitializeRoot(enum ErrorCode *error);

enum Register {
  // The global symbol table (hash -> symbol)
  REGISTER_SYMBOL_TABLE,
  // Registers for reading
  REGISTER_READ_SOURCE,
  REGISTER_READ_STACK,
  REGISTER_READ_RESULT,

  // Registers for evaluation
  REGISTER_STACK,
  REGISTER_EXPRESSION,
  REGISTER_VALUE,
  REGISTER_ENVIRONMENT,
  REGISTER_ARGUMENT_LIST,
  REGISTER_PROCEDURE,
  REGISTER_UNEVALUATED,
  REGISTER_CONTINUE,

  // Total number of registers
  NUM_REGISTERS,
};

Object GetRegister(enum Register reg);
void SetRegister(enum Register reg, Object value);

void Save(enum Register reg, enum ErrorCode *error);
void Restore(enum Register reg);

EvaluateFunction GetContinue();
void SetContinue(EvaluateFunction func);

Object GetValue();
void SetValue(Object value);

Object GetExpression();
void SetExpression(Object expression);

Object GetEnvironment();
void SetEnvironment(Object environment);

Object GetUnevaluated();
void SetUnevaluated(Object unevaluated);

Object GetProcedure();
void SetProcedure(Object procedure);

Object GetArgumentList();
void SetArgumentList(Object arguments);

EvaluateFunction GetContinue();
void SetContinue(EvaluateFunction func);

Object GetValue();
void SetValue(Object value);

Object GetExpression();
void SetExpression(Object expression);

Object GetEnvironment();
void SetEnvironment(Object environment);

Object GetUnevaluated();
void SetUnevaluated(Object unevaluated);

Object GetProcedure();
void SetProcedure(Object procedure);

Object GetArgumentList();
void SetArgumentList(Object arguments);

#endif
