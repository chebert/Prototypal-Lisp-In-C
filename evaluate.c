#include "evaluate.h"

#include <assert.h>

#include "compound_procedure.h"
#include "log.h"
#include "memory.h"
#include "pair.h"
#include "root.h"
#include "read.h"
#include "string.h"
#include "symbol_table.h"

b64 IsTaggedList(Object list, const char *tag);
b64 IsSelfEvaluating(Object expression);
b64 IsVariable(Object expression);
b64 IsQuoted(Object expression);
b64 IsAssignment(Object expression);
b64 IsDefinition(Object expression);
b64 IsIf(Object expression);
b64 IsLambda(Object expression);
b64 IsSequence(Object expression);
b64 IsApplication(Object expression);

void EvaluateSelfEvaluating();
void EvaluateVariable();
void EvaluateQuoted();
void EvaluateAssignment();
void EvaluateDefinition();
void EvaluateIf();
void EvaluateLambda();
void EvaluateBegin();
void EvaluateSequence();
void EvaluateApplication();
void EvaluateApplicationDispatch();
void EvaluateUnknown();

// Environment
Object LookupVariableValue(Object variable, Object environment, b64 *found);
void SetVariableValue(Object variable, Object value, Object environment);
void DefineVariable(Object variable, Object value, Object environment);
Object ExtendEnvironment(Object parameters, Object arguments, Object environment);

// Lambda
Object LambdaParameters(Object expression);
Object LambdaBody(Object expression);

// Procedures
Object ApplyPrimitiveProcedure(Object procedure, Object arguments);

// Assignment
Object AssignmentVariable(Object expression);
Object AssignmentValue(Object expression);

// Definition
Object DefinitionVariable(Object expression);
Object DefinitionValue(Object expression);

// If
Object IfPredicate(Object expression);
Object IfConsequent(Object expression);
Object IfAlternative(Object expression);
b64 IsTruthy(Object condition);

// Application
Object Operands(Object application);
Object Operator(Object application);
b64 HasNoOperands(Object operands);
Object EmptyArgumentList();
Object FirstOperand(Object operands);
Object RestOperands(Object operands);
b64 IsLastOperand(Object operands);

void PushValueOntoArgumentList();

// Sequence
Object FirstExpression(Object sequence);
Object RestExpressions(Object sequence);
b64 IsLastExpression(Object sequence);

// TODO: make continue register (subvert C call/return)

void Evaluate() {
  // TODO: environment needs
  //   - symbols interned: quote, set!, define, if, fn, ok
  Object expression = GetRegister(REGISTER_EXPRESSION);
  if (IsSelfEvaluating(expression)) {
    EvaluateSelfEvaluating();
  } else if (IsVariable(expression)) {
    EvaluateVariable();
  } else if (IsQuoted(expression)) {
    EvaluateQuoted();
  } else if (IsAssignment(expression)) {
    EvaluateAssignment();
  } else if (IsDefinition(expression)) {
    EvaluateDefinition();
  } else if (IsIf(expression)) {
    EvaluateIf();
  } else if (IsLambda(expression)) {
    EvaluateLambda();
  } else if (IsSequence(expression)) {
    EvaluateBegin();
  } else if (IsApplication(expression)) {
    EvaluateApplication();
  } else {
    EvaluateUnknown();
  }
}

b64 IsSelfEvaluating(Object expression) {
  return IsNil(expression)
    || IsTrue(expression)
    || IsFalse(expression)
    || IsFixnum(expression)
    || IsReal32(expression)
    || IsReal64(expression)
    || IsVector(expression)
    || IsByteVector(expression)
    || IsString(expression);
}

b64 IsTaggedList(Object list, const char *tag) {
  return IsPair(list) && FindSymbol(tag) == Car(list);
}

b64 IsVariable(Object expression)    { return IsSymbol(expression); }
b64 IsQuoted(Object expression)      { return IsTaggedList(expression, "quote"); }
b64 IsAssignment(Object expression)  { return IsTaggedList(expression, "set!"); }
b64 IsDefinition(Object expression)  { return IsTaggedList(expression, "define"); }
b64 IsIf(Object expression)          { return IsTaggedList(expression, "if"); }
b64 IsSequence(Object expression)    { return IsTaggedList(expression, "begin"); }
b64 IsLambda(Object expression)      { return IsTaggedList(expression, "fn"); }
b64 IsApplication(Object expression) { return IsPair(expression); }

void EvaluateSelfEvaluating() {
  Object expression = GetRegister(REGISTER_EXPRESSION);
  SetRegister(REGISTER_VALUE, expression);
}
void EvaluateVariable() {
  Object expression = GetRegister(REGISTER_EXPRESSION);
  b64 found = 0;
  Object value = LookupVariableValue(expression, GetRegister(REGISTER_ENVIRONMENT), &found);
  if (!found) {
    LOG_ERROR("Could not find %s in environment", StringCharacterBuffer(expression));
  }
  SetRegister(REGISTER_VALUE, value);
}
void EvaluateQuoted() {
  Object expression = GetRegister(REGISTER_EXPRESSION);
  SetRegister(REGISTER_VALUE, Cdr(expression));
}
void EvaluateLambda() {
  Object expression = GetRegister(REGISTER_EXPRESSION);
  SetRegister(REGISTER_UNEVALUATED, LambdaParameters(expression));
  SetRegister(REGISTER_EXPRESSION, LambdaBody(expression));

  Object procedure = AllocateCompoundProcedure();
  SetProcedureEnvironment(procedure, GetRegister(REGISTER_ENVIRONMENT));
  SetProcedureParameters(procedure, GetRegister(REGISTER_UNEVALUATED));
  SetProcedureBody(procedure, GetRegister(REGISTER_EXPRESSION));

  SetRegister(REGISTER_VALUE, procedure);
}

void EvaluateApplication() {
  Object expression = GetRegister(REGISTER_EXPRESSION);
  Save(REGISTER_ENVIRONMENT);
  SetRegister(REGISTER_UNEVALUATED, Operands(expression));
  Save(REGISTER_UNEVALUATED);
  // Evaluate the operator
  SetRegister(REGISTER_EXPRESSION, Operator(expression));
  Evaluate();

  // operator has been evaluated.
  Restore(REGISTER_UNEVALUATED);
  Restore(REGISTER_ENVIRONMENT);

  SetRegister(REGISTER_PROCEDURE, GetRegister(REGISTER_VALUE));
  SetRegister(REGISTER_ARGUMENT_LIST, EmptyArgumentList());
  if (HasNoOperands(GetRegister(REGISTER_UNEVALUATED))) {
    EvaluateApplicationDispatch();
    return;
  }

  Save(REGISTER_PROCEDURE);
  // Evaluate operands
  while (1) {
    Save(REGISTER_ARGUMENT_LIST);

    SetRegister(REGISTER_EXPRESSION, FirstOperand(GetRegister(REGISTER_UNEVALUATED)));
    if (IsLastOperand(GetRegister(REGISTER_UNEVALUATED))) {
      // Evaluate the last argument
      Evaluate();

      Restore(REGISTER_ARGUMENT_LIST);
      // Add it to the argument list.
      PushValueOntoArgumentList();
      // Perform the application
      EvaluateApplicationDispatch();
      return;
    }

    Save(REGISTER_ENVIRONMENT);
    Save(REGISTER_UNEVALUATED);
    // Evaluate the next operand.
    Evaluate();

    Restore(REGISTER_UNEVALUATED);
    Restore(REGISTER_ENVIRONMENT);

    Restore(REGISTER_ARGUMENT_LIST);

    // Add the evaluated argument to the argument list.
    PushValueOntoArgumentList();
    // Remove the evaluated operand from the unevaluated arguments
    SetRegister(REGISTER_UNEVALUATED, RestOperands(GetRegister(REGISTER_UNEVALUATED)));
  }
}

void EvaluateApplicationDispatch() {
  Object proc = GetRegister(REGISTER_PROCEDURE);
  if (IsPrimitiveProcedure(proc)) {
    SetRegister(REGISTER_VALUE, ApplyPrimitiveProcedure(proc, GetRegister(REGISTER_ARGUMENT_LIST)));
  } else if (IsCompoundProcedure(proc)) {
    Object params = ProcedureParameters(proc);
    Object environment = ProcedureEnvironment(proc);
    SetRegister(REGISTER_UNEVALUATED, params);
    SetRegister(REGISTER_ENVIRONMENT, environment);
    SetRegister(REGISTER_ENVIRONMENT,
        ExtendEnvironment(params, GetRegister(REGISTER_ARGUMENT_LIST), environment));

    SetRegister(REGISTER_UNEVALUATED, ProcedureBody(proc));
    EvaluateSequence();
  } else {
    LOG_ERROR("Unknown procedure type");
  }
}

void EvaluateAssignment() {
  Object expression = GetRegister(REGISTER_EXPRESSION);
  SetRegister(REGISTER_UNEVALUATED, AssignmentVariable(expression));
  Save(REGISTER_UNEVALUATED);
  SetRegister(REGISTER_EXPRESSION, AssignmentValue(expression));
  Save(REGISTER_EXPRESSION);
  Save(REGISTER_ENVIRONMENT);
  // Evaluate the assignment value
  Evaluate();
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_EXPRESSION);
  Restore(REGISTER_UNEVALUATED);
  SetVariableValue(
      GetRegister(REGISTER_UNEVALUATED),
      GetRegister(REGISTER_VALUE),
      GetRegister(REGISTER_ENVIRONMENT));
  // Return the symbol 'ok as the result of an assignment
  SetRegister(REGISTER_VALUE, FindSymbol("ok"));
}

void EvaluateDefinition() {
  Object expression = GetRegister(REGISTER_EXPRESSION);
  SetRegister(REGISTER_UNEVALUATED, DefinitionVariable(expression));
  Save(REGISTER_UNEVALUATED);
  SetRegister(REGISTER_EXPRESSION, DefinitionValue(expression));
  Save(REGISTER_EXPRESSION);
  Save(REGISTER_ENVIRONMENT);
  // Evaluate the definition value
  Evaluate();
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_EXPRESSION);
  Restore(REGISTER_UNEVALUATED);
  DefineVariable(
      GetRegister(REGISTER_UNEVALUATED),
      GetRegister(REGISTER_VALUE),
      GetRegister(REGISTER_ENVIRONMENT));
  // Return the symbol name as the result of the definition.
  SetRegister(REGISTER_VALUE, GetRegister(REGISTER_UNEVALUATED));
}

void EvaluateIf() {
  Object expression = GetRegister(REGISTER_EXPRESSION);
  Save(REGISTER_EXPRESSION);
  Save(REGISTER_ENVIRONMENT);
  SetRegister(REGISTER_EXPRESSION, IfPredicate(GetRegister(REGISTER_EXPRESSION)));
  // Evaluate the predicate
  Evaluate();
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_EXPRESSION);
  if (IsTruthy(GetRegister(REGISTER_VALUE))) {
    // Consequent
    SetRegister(REGISTER_EXPRESSION, IfConsequent(GetRegister(REGISTER_EXPRESSION)));
  } else {
    // alternative
    SetRegister(REGISTER_EXPRESSION, IfAlternative(GetRegister(REGISTER_EXPRESSION)));
  }
  // Evaluate the consequent/alternative
  Evaluate();
}

void EvaluateBegin() {
  Object expression = GetRegister(REGISTER_EXPRESSION);
  SetRegister(REGISTER_UNEVALUATED, expression);
  EvaluateSequence();
}
void EvaluateSequence() {
  while (1) {
    Object unevaluated = GetRegister(REGISTER_UNEVALUATED);
    SetRegister(REGISTER_EXPRESSION, FirstExpression(unevaluated));
    if (IsLastExpression(unevaluated)) {
      // Last expression
      Evaluate();
      return;
    }
    Save(REGISTER_UNEVALUATED);
    Save(REGISTER_ENVIRONMENT);

    Evaluate();

    Restore(REGISTER_ENVIRONMENT);
    Restore(REGISTER_UNEVALUATED);

    SetRegister(REGISTER_UNEVALUATED, RestExpressions(GetRegister(REGISTER_UNEVALUATED)));
  }
}

void EvaluateUnknown() {
  PrintlnObject(GetRegister(REGISTER_EXPRESSION));
  assert(!"Unknown Expression");
}

Object Second(Object list) { return Car(Cdr(list)); }
Object Third(Object list) { return Car(Cdr(Cdr(list))); }
Object Fourth(Object list) { return Car(Cdr(Cdr(Cdr(list)))); }

Object LookupVariableValue(Object variable, Object environment, b64 *found) { 
  assert(!"TODO");
}
void SetVariableValue(Object variable, Object value, Object environment) { 
  assert(!"TODO");
}
void DefineVariable(Object variable, Object value, Object environment) { 
  assert(!"TODO");
}
Object ExtendEnvironment(Object parameters, Object arguments, Object environment) { 
  assert(!"TODO");
}

Object ApplyPrimitiveProcedure(Object procedure, Object arguments) { 
  PrimitiveFunction function = UnboxPrimitiveProcedure(procedure);
  return function(arguments);
}

Object LambdaParameters(Object lambda) {
  return Second(lambda);
}
Object LambdaBody(Object lambda) {
  return Cdr(Cdr(lambda));
}

// Assignment
Object AssignmentVariable(Object expression) {
  return Second(expression);
}
Object AssignmentValue(Object expression) {
  return Third(expression);
}

// Definition
Object DefinitionVariable(Object expression) {
  return Second(expression);
}
Object DefinitionValue(Object expression) {
  return Third(expression);
}

// If
Object IfPredicate(Object expression) {
  return Second(expression);
}
Object IfConsequent(Object expression) {
  return Third(expression);
}
Object IfAlternative(Object expression) {
  return Fourth(expression);
}
b64 IsTruthy(Object condition) {
  return !IsFalse(condition);
}

// Application
Object Operator(Object application) { return Car(application); }
Object Operands(Object application) { return Cdr(application); }

b64 HasNoOperands(Object operands) { return operands == nil; }
Object EmptyArgumentList() { return nil; }
Object FirstOperand(Object operands) { return Car(operands); }
Object RestOperands(Object operands) { return Cdr(operands); }
b64 IsLastOperand(Object operands) { return HasNoOperands(RestOperands(operands)); }

void PushValueOntoArgumentList() {
  Object pair = AllocatePair();
  SetCar(pair, GetRegister(REGISTER_VALUE));
  SetCdr(pair, GetRegister(REGISTER_ARGUMENT_LIST));
  SetRegister(REGISTER_ARGUMENT_LIST, GetRegister(REGISTER_ARGUMENT_LIST));
}

// Sequence
Object FirstExpression(Object sequence) { return First(sequence); }
Object RestExpressions(Object sequence) { return Rest(sequence); }
b64 IsLastExpression(Object sequence) { return Rest(sequence) == nil; }

void TestEvaluate() {
}
