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

void EvaluateError();

static Object MakeProcedure(enum ErrorCode *error);

Object Second(Object list);
Object Third(Object list);
Object Fourth(Object list);

// Lambda
Object LambdaParameters(Object expression);
Object LambdaBody(Object expression);

// Procedures
Object ApplyPrimitiveProcedure(Object procedure, Object arguments, enum ErrorCode *error);

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
void AdjoinArgument(enum ErrorCode *error);

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

#define CHECK(error) \
  do { if (error) { next = EvaluateError; return; } } while(0)

#define SAVE_AND_CHECK(reg, error) \
  do { Save(reg, &error); CHECK(error); } while(0)

static EvaluateFunction next = 0;
static enum ErrorCode error = NO_ERROR;

// Primitives
Object PrimitiveAddFixnumFixnum(Object arguments, enum ErrorCode *error);
Object PrimitiveSubtractFixnumFixnum(Object arguments, enum ErrorCode *error);

#define PRIMITIVES \
  X("+", PrimitiveAddFixnumFixnum) \
  X("-", PrimitiveSubtractFixnumFixnum)

const u8 *primitive_names[] = {
#define X(name, function) name,
  PRIMITIVES
#undef X
};
PrimitiveFunction primitives[] = {
#define X(name, function) function,
  PRIMITIVES
#undef X
};
#define NUM_PRIMITIVES (sizeof(primitives) / sizeof(primitives[0]))

void DefinePrimitive(const u8 *name, PrimitiveFunction function) {
  enum ErrorCode error;
  SetUnevaluated(InternSymbol(name, &error));
  assert(!error);
  SetValue(BoxPrimitiveProcedure(function));
  DefineVariable(&error);
  assert(!error);
}

Object Evaluate(Object expression) {
  // Set the expression to be evaluated.
  SetExpression(expression);

  // Clear the stack.
  SetRegister(REGISTER_STACK, nil);

  // Ensure that the evaluator can find all the necessary symbols.
  // to avoid allocating during EvaluateDispatch.
  InternSymbol("quote", &error);
  InternSymbol("set!", &error);
  InternSymbol("define", &error);
  InternSymbol("if", &error);
  InternSymbol("fn", &error);
  InternSymbol("begin", &error);
  InternSymbol("ok", &error);

  // Create the initial environment
  MakeInitialEnvironment(&error);

  // Add primitive functions to the initial environment
  for (int i = 0; i < NUM_PRIMITIVES; ++i)
    DefinePrimitive(primitive_names[i], primitives[i]);

  // Set continue to quit when evaluation finishes.
  SetContinue(0);

  // Start the evaluation by evaluating the provided expression.
  next = EvaluateDispatch;

  // Run the evaluation loop until quit.
  while (next)
    next();

  // Return the evaluated expression.
  return GetValue();
}

void EvaluateDispatch() {
  Object expression = GetExpression();
  LOG(LOG_EVALUATE, "Evaluating expression:");
  LOG_OP(LOG_EVALUATE, PrintlnObject(expression));
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
  LOG(LOG_EVALUATE, "Self evaluating");
  SetValue(GetExpression());
  next = GetContinue();
}
void EvaluateVariable() {
  b64 found = 0;
  LOG(LOG_EVALUATE, "Looking up variable value in environment: ");
  LOG_OP(LOG_EVALUATE, PrintlnObject(GetExpression()));
  LOG_OP(LOG_EVALUATE, PrintlnObject(GetEnvironment()));
  Object value = LookupVariableValue(GetExpression(), GetEnvironment(), &found);
  if (!found) {
    LOG_ERROR("Could not find %s in environment", StringCharacterBuffer(GetExpression()));
    error = ERROR_EVALUATE_UNBOUND_VARIABLE;
    next = EvaluateError;
  } else {
    SetValue(value);
    next = GetContinue();
  }
}
void EvaluateQuoted() {
  LOG(LOG_EVALUATE, "Quoted expression");
  SetValue(Second(GetExpression()));
  next = GetContinue();
}
void EvaluateLambda() {
  LOG(LOG_EVALUATE, "Lambda expression");
  SetUnevaluated(LambdaParameters(GetExpression()));
  SetExpression(LambdaBody(GetExpression()));
  LOG(LOG_EVALUATE, "Lambda body: ");
  LOG_OP(LOG_EVALUATE, PrintlnObject(GetExpression()));
  SetValue(MakeProcedure(&error));
  CHECK(error);
  next = GetContinue();
}

void EvaluateApplication() {
  LOG(LOG_EVALUATE, "Application");
  SAVE_AND_CHECK(REGISTER_CONTINUE, error);
  SAVE_AND_CHECK(REGISTER_ENVIRONMENT, error);
  SetUnevaluated(Operands(GetExpression()));
  SAVE_AND_CHECK(REGISTER_UNEVALUATED, error);
  // Evaluate the operator
  LOG(LOG_EVALUATE, "Evaluating operator");
  SetExpression(Operator(GetExpression()));
  SetContinue(EvaluateApplicationOperands);
  next = EvaluateDispatch;
}

void EvaluateApplicationOperands() {
  // operator has been evaluated.
  Restore(REGISTER_UNEVALUATED);
  Restore(REGISTER_ENVIRONMENT);

  SetProcedure(GetValue());
  SetArgumentList(EmptyArgumentList());

  LOG(LOG_EVALUATE, "Evaluating operands");
  if (HasNoOperands(GetUnevaluated())) {
    // CASE: No operands
    LOG(LOG_EVALUATE, "No operands");
    next = EvaluateApplicationDispatch;
  } else {
    // CASE: 1 or more operands
    SAVE_AND_CHECK(REGISTER_PROCEDURE, error);
    next = EvaluateApplicationOperandLoop;
  }
}

Object SetLastCdr(Object list, Object last_pair) {
  // List is empty. The resulting list is just last_pair.
  if (IsNil(list)) return last_pair;

  Object head = list;
  for (; IsPair(Cdr(list)); list = Cdr(list))
    ;
  SetCdr(list, last_pair);
  return head;
}

void AdjoinArgument(enum ErrorCode *error) {
  Object last_pair = AllocatePair(error);
  if (*error) return;

  SetCar(last_pair, GetValue());
  SetArgumentList(SetLastCdr(GetArgumentList(), last_pair));
}

void EvaluateApplicationAccumulateArgument() {
  LOG(LOG_EVALUATE, "Accumulating argument");
  Restore(REGISTER_UNEVALUATED);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_ARGUMENT_LIST);
  AdjoinArgument(&error);
  CHECK(error);
  SetUnevaluated(RestOperands(GetUnevaluated()));
  next = EvaluateApplicationOperandLoop;
}

void EvaluateApplicationAccumulateLastArgument() {
  LOG(LOG_EVALUATE, "Accumulating last argument");
  Restore(REGISTER_ARGUMENT_LIST);
  AdjoinArgument(&error);
  CHECK(error);
  Restore(REGISTER_PROCEDURE);
  next = EvaluateApplicationDispatch;
}

void EvaluateApplicationLastOperand() {
  LOG(LOG_EVALUATE, "last operand");
  SetContinue(EvaluateApplicationAccumulateLastArgument);
  next = EvaluateDispatch;
}

void EvaluateApplicationOperandLoop() {
  LOG(LOG_EVALUATE, "Evaluating next operand");
  // Evaluate operands
  SAVE_AND_CHECK(REGISTER_ARGUMENT_LIST, error);

  SetExpression(FirstOperand(GetUnevaluated()));

  if (IsLastOperand(GetUnevaluated())) {
    // CASE: 1 argument
    next = EvaluateApplicationLastOperand;
  } else {
    // CASE: 2+ arguments
    LOG(LOG_EVALUATE, "not the last operand");
    SAVE_AND_CHECK(REGISTER_ENVIRONMENT, error);
    SAVE_AND_CHECK(REGISTER_UNEVALUATED, error);
    SetContinue(EvaluateApplicationAccumulateArgument);
    next = EvaluateDispatch;
  }
}

void EvaluateApplicationDispatch() {
  LOG(LOG_EVALUATE, "Evaluate application: dispatch based on primitive/compound procedure");
  Object proc = GetProcedure();
  if (IsPrimitiveProcedure(proc)) {
    // Primitive-procedure application
    SetValue(ApplyPrimitiveProcedure(proc, GetArgumentList(), &error));
    CHECK(error);
    Restore(REGISTER_CONTINUE);
    next = GetContinue();
  } else if (IsCompoundProcedure(proc)) {
    // Compound-procedure application
    SetUnevaluated(ProcedureParameters(proc));
    SetEnvironment(ProcedureEnvironment(proc));
    LOG(LOG_EVALUATE, "Extending the environment");
    ExtendEnvironment(&error);
    CHECK(error);

    proc = GetProcedure();
    SetUnevaluated(ProcedureBody(proc));
    next = EvaluateSequence;
  } else {
    error = ERROR_EVALUATE_UNKNOWN_PROCEDURE_TYPE;
    next = EvaluateError;
  }
}

void EvaluateBegin() {
  LOG(LOG_EVALUATE, "Evaluate begin");
  SetUnevaluated(BeginActions(GetExpression()));
  SAVE_AND_CHECK(REGISTER_CONTINUE, error);
  next = EvaluateSequence;
}

void EvaluateSequenceContinue() {
  LOG(LOG_EVALUATE, "Evaluate next expression in sequence");
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_UNEVALUATED);

  SetUnevaluated(RestExpressions(GetUnevaluated()));
  next = EvaluateSequence;
}
void EvaluateSequenceLastExpression() {
  LOG(LOG_EVALUATE, "Evaluate sequence, last expression");
  Restore(REGISTER_CONTINUE);
  next = EvaluateDispatch;
}

void EvaluateSequence() {
  LOG(LOG_EVALUATE, "Evaluate sequence");
  Object unevaluated = GetUnevaluated();
  LOG_OP(LOG_EVALUATE, PrintlnObject(unevaluated));
  SetExpression(FirstExpression(unevaluated));

  if (IsLastExpression(unevaluated)) {
    // Case: 1 expression left to evaluate
    next = EvaluateSequenceLastExpression;
  } else {
    // Case: 2+ expressions left to evaluate
    SAVE_AND_CHECK(REGISTER_UNEVALUATED, error);
    SAVE_AND_CHECK(REGISTER_ENVIRONMENT, error);
    SetContinue(EvaluateSequenceContinue);
    next = EvaluateDispatch;
  }
}

void EvaluateIfDecide() {
  LOG(LOG_EVALUATE, "Evaluate if, condition already evaluated");
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
  LOG(LOG_EVALUATE, "Evaluate if");
  SAVE_AND_CHECK(REGISTER_EXPRESSION, error);
  SAVE_AND_CHECK(REGISTER_ENVIRONMENT, error);
  SAVE_AND_CHECK(REGISTER_CONTINUE, error);
  SetContinue(EvaluateIfDecide);
  SetExpression(IfPredicate(GetExpression()));
  next = EvaluateDispatch;
}


void EvaluateAssignment1() {
  LOG(LOG_EVALUATE, "Evaluate assignment, value already evaluated");
  Restore(REGISTER_CONTINUE);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_UNEVALUATED);

  SetVariableValue(
      GetUnevaluated(),
      GetValue(),
      GetEnvironment(),
      &error);
  CHECK(error);
  // Return the symbol 'ok as the result of an assignment
  SetValue(FindSymbol("ok"));
  next = GetContinue();
}

void EvaluateAssignment() {
  LOG(LOG_EVALUATE, "Evaluate assignment");
  SetUnevaluated(AssignmentVariable(GetExpression()));
  SAVE_AND_CHECK(REGISTER_UNEVALUATED, error);

  SetExpression(AssignmentValue(GetExpression()));
  SAVE_AND_CHECK(REGISTER_ENVIRONMENT, error);
  SAVE_AND_CHECK(REGISTER_CONTINUE, error);
  SetContinue(EvaluateAssignment1);
  next = EvaluateDispatch;
}

void EvaluateDefinition1() {
  LOG(LOG_EVALUATE, "Current stack: ");
  LOG_OP(LOG_EVALUATE, PrintlnObject(GetRegister(REGISTER_STACK)));
  Restore(REGISTER_CONTINUE);
  LOG(LOG_EVALUATE, "Restored continue");
  Restore(REGISTER_ENVIRONMENT);
  LOG(LOG_EVALUATE, "Restored environment");
  Restore(REGISTER_UNEVALUATED);
  LOG(LOG_EVALUATE, "Restored unevaluated");
  DefineVariable(&error);
  CHECK(error);
  LOG(LOG_EVALUATE, "defining variable");
  LOG_OP(LOG_EVALUATE, PrintlnObject(GetEnvironment()));
  // Return the symbol name as the result of the definition.
  SetValue(GetUnevaluated());
  LOG(LOG_EVALUATE, "Setting the value");
  LOG_OP(LOG_EVALUATE, PrintlnObject(GetValue()));
  next = GetContinue();
}

void EvaluateDefinition() {
  SetUnevaluated(DefinitionVariable(GetExpression()));
  SAVE_AND_CHECK(REGISTER_UNEVALUATED, error);

  LOG(LOG_EVALUATE, "Setting, then saving Unevaluated:");
  LOG_OP(LOG_EVALUATE, PrintlnObject(GetUnevaluated()));

  SetExpression(DefinitionValue(GetExpression()));

  LOG(LOG_EVALUATE, "Setting the expression:");
  LOG_OP(LOG_EVALUATE, PrintlnObject(GetExpression()));

  SAVE_AND_CHECK(REGISTER_ENVIRONMENT, error);
  SAVE_AND_CHECK(REGISTER_CONTINUE, error);
  SetContinue(EvaluateDefinition1);

  LOG(LOG_EVALUATE, "Setting continue to %x", EvaluateDefinition1);

  next = EvaluateDispatch;
}

void EvaluateUnknown() {
  LOG_ERROR("Unknown Expression");
  LOG_OP(LOG_EVALUATE, PrintlnObject(GetExpression()));
  error = ERROR_EVALUATE_UNKNOWN_EXPRESSION;
  next = EvaluateError;
}

void EvaluateError() {
  LOG_ERROR("%s", ErrorCodeString(error));
  next = 0;
}

Object Second(Object list) { return Car(Cdr(list)); }
Object Third(Object list) { return Car(Cdr(Cdr(list))); }
Object Fourth(Object list) { return Car(Cdr(Cdr(Cdr(list)))); }

Object ApplyPrimitiveProcedure(Object procedure, Object arguments, enum ErrorCode *error) { 
  LOG(LOG_EVALUATE, "applying primitive function");
  LOG_OP(LOG_EVALUATE, PrintlnObject(procedure));
  LOG_OP(LOG_EVALUATE, PrintlnObject(arguments));
  PrimitiveFunction function = UnboxPrimitiveProcedure(procedure);
  return function(arguments, error);
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

Object MakeProcedure(enum ErrorCode *error) {
  Object procedure = AllocateCompoundProcedure(error);
  if (*error) return nil;

  SetProcedureEnvironment(procedure, GetEnvironment());
  SetProcedureParameters(procedure, GetUnevaluated());
  SetProcedureBody(procedure, GetExpression());
  return procedure;
}

Object PrimitiveAddFixnumFixnum(Object arguments, enum ErrorCode *error) {
  if (IsNil(arguments)) {
    *error = ERROR_EVALUATE_ARITY_MISMATCH;
    return nil;
  }
  Object a = First(arguments);
  if (!IsFixnum(a)) {
    *error = ERROR_EVALUATE_INVALID_ARGUMENT_TYPE;
    return nil;
  }
  arguments = Rest(arguments);
  if (IsNil(arguments)) {
    *error = ERROR_EVALUATE_ARITY_MISMATCH;
    return nil;
  }
  Object b = First(arguments);
  if (!IsFixnum(b)) {
    *error = ERROR_EVALUATE_INVALID_ARGUMENT_TYPE;
    return nil;
  }

  return BoxFixnum(UnboxFixnum(a) + UnboxFixnum(b));
}

Object PrimitiveSubtractFixnumFixnum(Object arguments, enum ErrorCode *error) {
  if (IsNil(arguments)) {
    *error = ERROR_EVALUATE_ARITY_MISMATCH;
    return nil;
  }
  Object a = First(arguments);
  if (!IsFixnum(a)) {
    *error = ERROR_EVALUATE_INVALID_ARGUMENT_TYPE;
    return nil;
  }
  arguments = Rest(arguments);
  if (IsNil(arguments)) {
    *error = ERROR_EVALUATE_ARITY_MISMATCH;
    return nil;
  }
  Object b = First(arguments);
  if (!IsFixnum(b)) {
    *error = ERROR_EVALUATE_INVALID_ARGUMENT_TYPE;
    return nil;
  }
  return BoxFixnum(UnboxFixnum(a) - UnboxFixnum(b));
}

void TestEvaluate() {
  InitializeMemory(128, &error);
  InitializeSymbolTable(1, &error);

  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(BoxFixnum(42))));

  ReadObject("'(hello world)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(define x 42)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(begin 1 2 3)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(begin (define x 42) x)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(fn (x y z) z)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("((fn (x y z) z) 1 2 3)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("((fn () 3))", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(((fn (z) (fn () z)) 3))", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("+", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(+ 720 360)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(- (+ 720 360) 14)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(- \"hello\" 14)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(- 14)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  DestroyMemory();
}
