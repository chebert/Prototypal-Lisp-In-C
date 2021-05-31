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

Object Evaluate(Object expression);

// EvaluateFunctions
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

void EvaluateApplicationOperandLoop();
void EvaluateApplicationAccumulateArgument();
void EvaluateApplicationAccumulateLastArgument();
void EvaluateApplicationLastOperand();

void EvaluateSequenceContinue();
void EvaluateSequenceLastExpression();

void EvaluateIfDecide();
void EvaluateAssignment1();
void EvaluateDefinition1();

static Object MakeProcedure();

Object Second(Object list);
Object Third(Object list);
Object Fourth(Object list);

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
void AdjoinArgument();

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

static EvaluateFunction next;

Object AddFixnumFixnum(Object arguments) {
  Object a = First(arguments);
  Object b = Second(arguments);
  return BoxFixnum(UnboxFixnum(a) + UnboxFixnum(b));
}

Object Evaluate(Object expression) {
  SetExpression(expression);
  SetRegister(REGISTER_STACK, nil);
  InternSymbol("quote");
  InternSymbol("set!");
  InternSymbol("define");
  InternSymbol("if");
  InternSymbol("fn");
  InternSymbol("begin");
  InternSymbol("ok");
  // TODO: fill the global environment
  SetEnvironment(AllocatePair());
  SetCar(GetEnvironment(), AllocatePair());
  SetUnevaluated(InternSymbol("+"));
  SetValue(BoxPrimitiveProcedure(AddFixnumFixnum));
  DefineVariable(REGISTER_UNEVALUATED, REGISTER_VALUE, REGISTER_ENVIRONMENT);

  SetContinue(NULL);
  next = EvaluateDispatch;

  while (next)
    next();

  return GetValue();
}

void EvaluateDispatch() {
  Object expression = GetExpression();
  LOG("Evaluating expression:");
  PrintlnObject(expression);
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
  b64 found = 0;
  LOG("Looking up variable value in environment: ");
  PrintlnObject(GetExpression());
  PrintlnObject(GetEnvironment());
  Object value = LookupVariableValue(GetExpression(), GetEnvironment(), &found);
  if (!found) {
    LOG_ERROR("Could not find %s in environment", StringCharacterBuffer(GetExpression()));
  }
  SetValue(value);
  next = GetContinue();
}
void EvaluateQuoted() {
  Object expression = GetExpression();
  SetValue(Second(expression));
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
    next = EvaluateApplicationOperandLoop;
  }
}

Object SetLastCdr(Object list, Object last_pair) {
  if (IsNil(list)) return last_pair;
  Object head = list;

  for (; IsPair(Cdr(list)); list = Cdr(list))
    ;
  SetCdr(list, last_pair);
  return head;
}

void AdjoinArgument() {
  Object last_pair = AllocatePair();
  SetCar(last_pair, GetValue());
  Object arguments = SetLastCdr(GetArgumentList(), last_pair);
  SetArgumentList(arguments);
}

void EvaluateApplicationAccumulateArgument() {
  Restore(REGISTER_UNEVALUATED);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_ARGUMENT_LIST);
  AdjoinArgument();
  SetUnevaluated(RestOperands(GetUnevaluated()));
  next = EvaluateApplicationOperandLoop;
}

void EvaluateApplicationAccumulateLastArgument() {
  Restore(REGISTER_ARGUMENT_LIST);
  AdjoinArgument();
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
  LOG("Current stack: ");
  PrintlnObject(GetRegister(REGISTER_STACK));
  Restore(REGISTER_CONTINUE);
  LOG("Restored continue");
  Restore(REGISTER_ENVIRONMENT);
  LOG("Restored environment");
  Restore(REGISTER_UNEVALUATED);
  LOG("Restored unevaluated");
  DefineVariable(REGISTER_UNEVALUATED, REGISTER_VALUE, REGISTER_ENVIRONMENT);
  LOG("defining variable");
  PrintlnObject(GetEnvironment());
  // Return the symbol name as the result of the definition.
  SetValue(GetUnevaluated());
  LOG("Setting the value");
  PrintlnObject(GetValue());
  next = GetContinue();
}

void EvaluateDefinition() {
  SetUnevaluated(DefinitionVariable(GetExpression()));
  Save(REGISTER_UNEVALUATED);

  LOG("Setting, then saving Unevaluated:");
  PrintlnObject(GetUnevaluated());

  SetExpression(DefinitionValue(GetExpression()));

  LOG("Setting the expression:");
  PrintlnObject(GetExpression());

  Save(REGISTER_ENVIRONMENT);
  Save(REGISTER_CONTINUE);
  SetContinue(EvaluateDefinition1);

  LOG("Setting continue to %x", EvaluateDefinition1);

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
  InitializeMemory(128);
  InitializeSymbolTable(1);
  PrintlnObject(Evaluate(BoxFixnum(42)));

  b64 success;
  ReadObject("'(hello world)", &success);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("(define x 42)", &success);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("(begin 1 2 3)", &success);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("(begin (define x 42) x)", &success);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("(fn (x y z) z)", &success);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("((fn (x y z) z) 1 2 3)", &success);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("((fn () 3))", &success);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("(((fn (z) (fn () z)) 3))", &success);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("+", &success);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("(+ 720 360)", &success);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  DestroyMemory();
}
