#include "environment.h"

#include <assert.h>
#include <string.h>

#include "pair.h"
#include "string.h"

// Environment := (innermost-scope next-innermost-scope ... global-scope)
// Scope       := (variables . values)
// Variables   := (variable ...)
// Values      := (value    ...)

Object LookupVariableReference(Object variable, Object environment);
Object LookupVariableInScope(Object variable, Object scope);

Object LookupVariableValue(Object variable, Object environment, b64 *found) {
  Object pair = LookupVariableReference(variable, environment);
  if (IsNil(pair)) {
    *found = 0;
    return nil;
  }
  *found = 1;
  return Car(pair);
}

void SetVariableValue(Object variable, Object value, Object environment) {
  Object pair = LookupVariableReference(variable, environment);
  assert(!IsNil(pair));
  SetCar(pair, value);
}

void DefineVariable(enum Register variable, enum Register value, enum Register environment) {
  {
    Object variables = AllocatePair();
    SetCar(variables, GetRegister(variable));
    SetCdr(variables, Car(Car(GetRegister(environment))));
    SetCar(Car(GetRegister(environment)), variables);
  }
  {
    Object values = AllocatePair();
    SetCar(values, GetRegister(value));
    SetCdr(values, Cdr(Car(GetRegister(environment))));
    SetCdr(Car(GetRegister(environment)), values);
  }
}

void ExtendEnvironment(enum Register parameters, enum Register arguments, enum Register environment) {
  {
    Object new_environment = AllocatePair();
    SetCdr(new_environment, GetRegister(environment));
    SetRegister(environment, new_environment);
  }

  Object new_scope = AllocatePair();
  SetCar(new_scope, parameters);
  SetCdr(new_scope, arguments);

  SetCar(GetRegister(environment), new_scope);
}

Object LookupVariableReference(Object variable, Object environment) {
  for (; !IsNil(environment); environment = Cdr(environment)) {
    Object pair = LookupVariableInScope(Car(environment), variable);
    if (!IsNil(pair)) return pair;
  }
  return nil;
}

Object LookupVariableInScope(Object variable, Object scope) {
  Object variables = Car(scope);
  Object values = Cdr(scope);

  for (; !IsNil(variables); variables = Cdr(variables), values = Cdr(values)) {
    if (!strcmp(StringCharacterBuffer(variable), StringCharacterBuffer(Car(variables)))) {
      return values;
    }
  }
  return nil;
}
