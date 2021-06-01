#include "environment.h"

#include <assert.h>
#include <string.h>

#include "log.h"
#include "pair.h"
#include "string.h"

// Environment := (innermost-scope next-innermost-scope ... global-scope)
// Scope       := (variables . values)
// Variables   := (variable ...)
// Values      := (value    ...)

Object LookupVariableReference(Object variable, Object environment);
Object LookupVariableInScope(Object variable, Object scope);

// Constructor/accessors for scopes
Object AllocateScope(enum ErrorCode *error);
Object ScopeVariables(Object scope);
Object ScopeValues(Object scope);
void SetScopeVariables(Object scope, Object variables);
void SetScopeValues(Object scope, Object variables);

Object InnerScope(Object environment);
void SetInnerScope(Object environment, Object scope);

Object LookupVariableValue(Object variable, Object environment, b64 *found) {
  Object values = LookupVariableReference(variable, environment);
  if (IsNil(values)) {
    *found = 0;
    return nil;
  }
  *found = 1;
  return First(values);
}

void SetVariableValue(Object variable, Object value, Object environment) {
  Object values = LookupVariableReference(variable, environment);
  assert(!IsNil(values));
  SetCar(values, value);
}

void DefineVariable() {
  enum ErrorCode error;
  // Environment := (scope . more-scopes)
  // Scope       := (variables . values)
  {
    Object new_variables = AllocatePair(&error);
    if (error) {
      // TODO: pass error up
    } else {
      SetCar(new_variables, GetUnevaluated());
      Object inner_scope = InnerScope(GetEnvironment());
      SetCdr(new_variables, ScopeVariables(inner_scope));
      SetScopeVariables(inner_scope, new_variables);
    }
  }
  {
    Object new_values = AllocatePair(&error);
    if (error) {
      // TODO: pass error up
    } else {
      SetCar(new_values, GetValue());
      Object inner_scope = InnerScope(GetEnvironment());
      SetCdr(new_values, ScopeValues(inner_scope));
      SetScopeValues(inner_scope, new_values);
    }
  }
}

void ExtendEnvironment() {
  enum ErrorCode error;
  {
    Object new_environment = AllocatePair(&error);
    if (error) {
      // TODO: pass error up
    } else {
      SetCdr(new_environment, GetEnvironment());
      SetEnvironment(new_environment);
    }
  }

  Object new_scope = AllocateScope(&error);
  if (error) {
    // TODO: pass error up
  } else {
    SetScopeVariables(new_scope, GetUnevaluated());
    SetScopeValues(new_scope, GetArgumentList());

    SetInnerScope(GetEnvironment(), new_scope);
  }
}

void MakeInitialEnvironment() {
  enum ErrorCode error;
  SetEnvironment(AllocatePair(&error));
  if (error) {
    assert(!"Could not allocate initial environment");
  }
  SetInnerScope(GetEnvironment(), AllocateScope(&error));
  if (error) {
    assert(!"Could not allocate global scope");
  }
}

Object LookupVariableReference(Object variable, Object environment) {
  for (; !IsNil(environment); environment = Cdr(environment)) {
    Object scope = InnerScope(environment);
    Object values = LookupVariableInScope(variable, scope);
    if (!IsNil(values)) return values;
  }
  return nil;
}

Object LookupVariableInScope(Object variable, Object scope) {
  Object variables = ScopeVariables(scope);
  Object values = ScopeValues(scope);

  for (; !IsNil(variables); variables = Cdr(variables), values = Cdr(values)) {
    if (!strcmp(StringCharacterBuffer(variable), StringCharacterBuffer(Car(variables)))) {
      return values;
    }
  }
  return nil;
}

Object AllocateScope(enum ErrorCode *error) { return AllocatePair(error); }
Object ScopeVariables(Object scope) { return Car(scope); }
Object ScopeValues(Object scope) { return Cdr(scope); }

void SetScopeVariables(Object scope, Object variables) { SetCar(scope, variables); }
void SetScopeValues(Object scope, Object variables) { SetCdr(scope, variables); }

Object InnerScope(Object environment) { return First(environment); } 
void SetInnerScope(Object environment, Object scope) { SetCar(environment, scope); }
