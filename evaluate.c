#include "evaluate.h"

#include <assert.h>

#include "compound_procedure.h"
#include "environment.h"
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

void EvaluateDispatch();
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
void EvaluateApplicationOperands();
void EvaluateApplicationDispatch();
void EvaluateUnknown();

static Object MakeProcedure();

// Environment
Object LookupVariableValue(Object variable, Object environment, b64 *found);
void SetVariableValue(Object variable, Object value, Object environment);
void DefineVariable(enum Register variable, enum Register value, enum Register environment);

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

// Sequence
Object BeginActions(Object expression);
Object FirstExpression(Object sequence);
Object RestExpressions(Object sequence);
b64 IsLastExpression(Object sequence);

EvaluateFunction next;

// Registers

void Evaluate() {
  next = EvaluateDispatch;
  while (next)
    next();
}

void EvaluateDispatch() {
  // TODO: environment needs
  //   - symbols interned: quote, set!, define, if, fn, ok
  Object expression = GetExpression();
  if (IsSelfEvaluating(expression)) {
    next = EvaluateSelfEvaluating;
  } else if (IsVariable(expression)) {
    next = EvaluateVariable;
  } else if (IsQuoted(expression)) {
    next = EvaluateQuoted;
  } else if (IsAssignment(expression)) {
    next = EvaluateAssignment;
  } else if (IsDefinition(expression)) {
    next = EvaluateDefinition;
  } else if (IsIf(expression)) {
    next = EvaluateIf;
  } else if (IsLambda(expression)) {
    next = EvaluateLambda;
  } else if (IsSequence(expression)) {
    next = EvaluateBegin;
  } else if (IsApplication(expression)) {
    next = EvaluateApplication;
  } else {
    next = EvaluateUnknown;
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
  Object expression = GetExpression();
  SetValue(expression);
  next = GetContinue();
}
void EvaluateVariable() {
  Object expression = GetExpression();
  b64 found = 0;
  Object value = LookupVariableValue(expression, GetEnvironment(), &found);
  if (!found) {
    LOG_ERROR("Could not find %s in environment", StringCharacterBuffer(expression));
  }
  SetValue(value);
  next = GetContinue();
}
void EvaluateQuoted() {
  Object expression = GetExpression();
  SetValue(Cdr(expression));
  next = GetContinue();
}
void EvaluateLambda() {
  Object expression = GetExpression();
  SetUnevaluated(LambdaParameters(expression));
  SetExpression(LambdaBody(expression));
  SetValue(MakeProcedure());
  next = GetContinue();
}

void EvaluateApplication() {
  Object expression = GetExpression();
  Save(REGISTER_CONTINUE);
  Save(REGISTER_ENVIRONMENT);
  SetUnevaluated(Operands(expression));
  Save(REGISTER_UNEVALUATED);
  // Evaluate the operator
  SetExpression(Operator(expression));
  SetContinue(EvaluateApplicationOperands);
  next = EvaluateDispatch;
}

void EvaluateApplicationOperands() {
  // operator has been evaluated.
  Restore(REGISTER_UNEVALUATED);
  Restore(REGISTER_ENVIRONMENT);

  SetProcedure(GetValue());
  SetArgumentList(EmptyArgumentList());

  if (HasNoOperands(GetUnevaluated())) {
    // CASE: No operands
    next = EvaluateApplicationDispatch;
  } else {
    // CASE: 1 or more operands
    Save(REGISTER_PROCEDURE);
    next = EvaluateOperandLoop;
  }
}

void EvaluateApplicationAccumulateArgument() {
  Restore(REGISTER_UNEVALUATED);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_ARGUMENT_LIST);
  SetArgumentList(AdjoinArgument(GetValue(), GetArgumentList()));
  SetUnevaluated(RestOperands(GetUnevaluated()));
  next = EvaluateApplicationOperandLoop;
}

void EvaluateApplicationAccumulateLastArgument() {
  Restore(REGISTER_ARGUMENT_LIST);
  SetArgumentList(AdjoinArgument(GetValue(), GetArgumentList()));
  Restore(REGISTER_PROCEDURE);
  next = EvaluateApplicationDispatch;
}

void EvaluateApplicationLastOperand() {
  SetContinue(EvaluateApplicationAccumulateLastArgument);
  next = EvaluateDispatch;
}

void EvaluateApplicationOperandLoop() {
  // Evaluate operands
  Save(REGISTER_ARGUMENT_LIST);

  SetExpression(FirstOperand(GetUnevaluated()));

  if (IsLastOperand(GetUnevaluated())) {
    // CASE: 1 argument
    next = EvaluateApplicationLastOperand;
  } else {
    // CASE: 2+ arguments
    Save(REGISTER_ENVIRONMENT);
    Save(REGISTER_UNEVALUATED);
    SetContinue(EvaluateApplicationAccumulateArgument);
    next = EvaluateDispatch;
  }
}

void EvaluateApplicationDispatch() {
  Object proc = GetProcedure();
  if (IsPrimitiveProcedure(proc)) {
    // Primitive-procedure application
    SetValue(ApplyPrimitiveProcedure(proc, GetArgumentList()));
    Restore(REGISTER_CONTINUE);
    next = GetContinue();
  } else if (IsCompoundProcedure(proc)) {
    // Compound-procedure application
    SetUnevaluated(ProcedureParameters(proc));
    SetEnvironment(ProcedureEnvironment(proc));
    ExtendEnvironment(REGISTER_UNEVALUATED, REGISTER_ARGUMENT_LIST, REGISTER_ENVIRONMENT);

    SetUnevaluated(ProcedureBody(proc));
    next = EvaluateSequence;
  } else {
    LOG_ERROR("Unknown procedure type");
    next = NULL;
  }
}

void EvaluateBegin() {
  SetUnevaluated(BeginActions(GetExpression()));
  Save(REGISTER_CONTINUE);
  next = EvaluateSequence;
}

void EvaluateSequenceContinue() {
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_UNEVALUATED);

  SetUnevaluated(RestExpressions(GetUnevaluated()));
  next = EvaluateSequence;
}
void EvaluateSequenceLastExpression() {
  Restore(REGISTER_CONTINUE);
  next = EvaluateDispatch;
}

void EvaluateSequence() {
  Object unevaluated = GetUnevaluated();
  SetExpression(FirstExpression(unevaluated));

  if (IsLastExpression(unevaluated)) {
    // Case: 1 expression left to evaluate
    next = EvaluateSequenceLastExpression;
  } else {
    // Case: 2+ expressions left to evaluate
    Save(REGISTER_UNEVALUATED);
    Save(REGISTER_ENVIRONMENT);
    SetContinue(EvaluateSequenceContinue);
    next = EvaluateDispatch;
  }
}

void EvaluateIfDecide() {
  Restore(REGISTER_CONTINUE);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_EXPRESSION);

  if (IsTruthy(GetValue())) {
    SetExpression(IfConsequent(GetExpression()));
  } else {
    SetExpression(IfAlternative(GetExpression()));
  }
  next = EvaluateDispatch;
}

void EvaluateIf() {
  Save(REGISTER_EXPRESSION);
  Save(REGISTER_ENVIRONMENT);
  Save(REGISTER_CONTINUE);
  SetContinue(EvaluateIfDecide);
  SetExpression(IfPredicate(GetExpression()));
  next = EvaluateDispatch;
}

void EvaluateAssignment1() {
  Restore(REGISTER_CONTINUE);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_UNEVALUATED);

  SetVariableValue(
      GetUnevaluated(),
      GetValue(),
      GetEnvironment());
  // Return the symbol 'ok as the result of an assignment
  SetValue(FindSymbol("ok"));
  next = GetContinue();
}

void EvaluateAssignment() {
  SetUnevaluated(AssignmentVariable(GetExpression()));
  Save(REGISTER_UNEVALUATED);

  SetExpression(AssignmentValue(GetExpression()));
  Save(REGISTER_ENVIRONMENT);
  Save(REGISTER_CONTINUE);
  SetContinue(EvaluateAssignment1);
  next = EvaluateDispatch;
}

void EvaluateDefinition1() {
  Restore(REGISTER_CONTINUE);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_UNEVALUATED);
  DefineVariable(REGISTER_UNEVALUATED, REGISTER_VALUE, REGISTER_ENVIRONMENT);
  // Return the symbol name as the result of the definition.
  SetValue(GetUnevaluated());
  next = GetContinue();
}

void EvaluateDefinition() {
  SetUnevaluated(DefinitionVariable(GetExpression()));
  Save(REGISTER_UNEVALUATED);

  SetExpression(DefinitionValue(GetExpression()));
  Save(REGISTER_ENVIRONMENT);
  Save(REGISTER_CONTINUE);
  SetContinue(EvaluateDefinition1);
  next = EvaluateDispatch;
}

void EvaluateUnknown() {
  PrintlnObject(GetExpression());
  assert(!"Unknown Expression");
}

Object Second(Object list) { return Car(Cdr(list)); }
Object Third(Object list) { return Car(Cdr(Cdr(list))); }
Object Fourth(Object list) { return Car(Cdr(Cdr(Cdr(list)))); }

Object ApplyPrimitiveProcedure(Object procedure, Object arguments) { 
  PrimitiveFunction function = UnboxPrimitiveProcedure(procedure);
  return function(arguments);
}

// Lambda: (lambda (parameters...) . body...)
Object LambdaParameters(Object lambda) { return Second(lambda); }
Object LambdaBody(Object lambda)       { return Cdr(Cdr(lambda)); }

// Assignment: (set! variable value)
Object AssignmentVariable(Object expression) { return Second(expression); }
Object AssignmentValue(Object expression)    { return Third(expression); }

// Definition: (define variable value)
Object DefinitionVariable(Object expression) { return Second(expression); }
Object DefinitionValue(Object expression)    { return Third(expression); }

// If: (if condition consequent alternative)
Object IfPredicate(Object expression)   { return Second(expression); }
Object IfConsequent(Object expression)  { return Third(expression); }
Object IfAlternative(Object expression) { return Fourth(expression); }
b64    IsTruthy(Object condition)       { return !IsFalse(condition); }

// Application: (operator . operands...)
Object Operator(Object application) { return Car(application); }
Object Operands(Object application) { return Cdr(application); }

// Operands: (operands...)
Object EmptyArgumentList()            { return nil; }
Object FirstOperand(Object operands)  { return Car(operands); }
Object RestOperands(Object operands)  { return Cdr(operands); }
b64    HasNoOperands(Object operands) { return IsNil(operands); }
b64    IsLastOperand(Object operands) { return HasNoOperands(RestOperands(operands)); }

// Sequence: (expressions...)
Object BeginActions(Object expression)   { return Rest(expression); }
Object FirstExpression(Object sequence)  { return First(sequence); }
Object RestExpressions(Object sequence)  { return Rest(sequence); }
b64    IsLastExpression(Object sequence) { return Rest(sequence) == nil; }

Object MakeProcedure() {
  Object procedure = AllocateCompoundProcedure();
  SetProcedureEnvironment(procedure, GetEnvironment());
  SetProcedureParameters(procedure, GetUnevaluated());
  SetProcedureBody(procedure, GetExpression());
  return procedure;
}

void TestEvaluate() {
}
