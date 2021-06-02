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
enum ErrorCode AdjoinArgument();

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

#define SAVE(reg, error) \
  do { \
    Save(reg, &error); \
    if (error) { \
      next = EvaluateError; \
      return; \
    } \
  } while(0);

static EvaluateFunction next;

// Primitives
Object PrimitiveAddFixnumFixnum(Object arguments) {
  Object a = First(arguments);
  Object b = Second(arguments);
  return BoxFixnum(UnboxFixnum(a) + UnboxFixnum(b));
}

Object PrimitiveSubtractFixnumFixnum(Object arguments) {
  Object a = First(arguments);
  Object b = Second(arguments);
  return BoxFixnum(UnboxFixnum(a) - UnboxFixnum(b));
}

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
  DefineVariable(REGISTER_UNEVALUATED, REGISTER_VALUE, REGISTER_ENVIRONMENT);
}

Object Evaluate(Object expression) {
  // Set the expression to be evaluated.
  SetExpression(expression);

  // Clear the stack.
  SetRegister(REGISTER_STACK, nil);

  // Ensure that the evaluator can find all the necessary symbols.
  // to avoid allocating during EvaluateDispatch.
  enum ErrorCode error;
  InternSymbol("quote", &error);
  InternSymbol("set!", &error);
  InternSymbol("define", &error);
  InternSymbol("if", &error);
  InternSymbol("fn", &error);
  InternSymbol("begin", &error);
  InternSymbol("ok", &error);

  // Create the initial environment
  MakeInitialEnvironment();

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
  LOG("Self evaluating");
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
    next = EvaluateError;
  } else {
    SetValue(value);
    next = GetContinue();
  }
}
void EvaluateQuoted() {
  LOG("Quoted expression");
  Object expression = GetExpression();
  SetValue(Second(expression));
  next = GetContinue();
}
void EvaluateLambda() {
  LOG("Lambda expression");
  Object expression = GetExpression();
  SetUnevaluated(LambdaParameters(expression));
  SetExpression(LambdaBody(expression));
  LOG("Lambda body: ");
  PrintlnObject(GetExpression());
  enum ErrorCode error;
  SetValue(MakeProcedure(&error));
  if (error) {
    next = EvaluateError;
  } else {
    next = GetContinue();
  }
}

void EvaluateApplication() {
  LOG("Application");
  Object expression = GetExpression();
  enum ErrorCode error;
  SAVE(REGISTER_CONTINUE, error);
  SAVE(REGISTER_ENVIRONMENT, error);
  SetUnevaluated(Operands(expression));
  SAVE(REGISTER_UNEVALUATED, error);
  // Evaluate the operator
  LOG("Evaluating operator");
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

  LOG("Evaluating operands");
  if (HasNoOperands(GetUnevaluated())) {
    // CASE: No operands
    LOG("No operands");
    next = EvaluateApplicationDispatch;
  } else {
    // CASE: 1 or more operands
    enum ErrorCode error;
    SAVE(REGISTER_PROCEDURE, error);
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

enum ErrorCode AdjoinArgument() {
  enum ErrorCode error;
  Object last_pair = AllocatePair(&error);
  if (error) {
    LOG_ERROR("Could not allocate pair for new argument");
    return error;
  }

  SetCar(last_pair, GetValue());
  SetArgumentList(SetLastCdr(GetArgumentList(), last_pair));
  return error;
}

void EvaluateApplicationAccumulateArgument() {
  LOG("Accumulating argument");
  Restore(REGISTER_UNEVALUATED);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_ARGUMENT_LIST);
  enum ErrorCode error = AdjoinArgument();
  if (error) {
    LOG_ERROR("Could not adjoin argument");
    next = EvaluateError;
  } else {
    SetUnevaluated(RestOperands(GetUnevaluated()));
    next = EvaluateApplicationOperandLoop;
  }
}

void EvaluateApplicationAccumulateLastArgument() {
  LOG("Accumulating last argument");
  Restore(REGISTER_ARGUMENT_LIST);
  enum ErrorCode error = AdjoinArgument();
  if (error) {
    LOG_ERROR("Could not adjoin argument");
    next = EvaluateError;
  } else {
    Restore(REGISTER_PROCEDURE);
    next = EvaluateApplicationDispatch;
  }
}

void EvaluateApplicationLastOperand() {
  LOG("last operand");
  SetContinue(EvaluateApplicationAccumulateLastArgument);
  next = EvaluateDispatch;
}

void EvaluateApplicationOperandLoop() {
  LOG("Evaluating next operand");
  // Evaluate operands
  enum ErrorCode error;
  SAVE(REGISTER_ARGUMENT_LIST, error);

  SetExpression(FirstOperand(GetUnevaluated()));

  if (IsLastOperand(GetUnevaluated())) {
    // CASE: 1 argument
    next = EvaluateApplicationLastOperand;
  } else {
    // CASE: 2+ arguments
    LOG("not the last operand");
    SAVE(REGISTER_ENVIRONMENT, error);
    SAVE(REGISTER_UNEVALUATED, error);
    SetContinue(EvaluateApplicationAccumulateArgument);
    next = EvaluateDispatch;
  }
}

void EvaluateApplicationDispatch() {
  LOG("Evaluate application: dispatch based on primitive/compound procedure");
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
    LOG("Extending the environment");
    ExtendEnvironment();

    proc = GetProcedure();
    SetUnevaluated(ProcedureBody(proc));
    next = EvaluateSequence;
  } else {
    LOG_ERROR("Unknown procedure type");
    next = EvaluateError;
  }
}

void EvaluateBegin() {
  LOG("Evaluate begin");
  SetUnevaluated(BeginActions(GetExpression()));
  enum ErrorCode error;
  SAVE(REGISTER_CONTINUE, error);
  next = EvaluateSequence;
}

void EvaluateSequenceContinue() {
  LOG("Evaluate next expression in sequence");
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_UNEVALUATED);

  SetUnevaluated(RestExpressions(GetUnevaluated()));
  next = EvaluateSequence;
}
void EvaluateSequenceLastExpression() {
  LOG("Evaluate sequence, last expression");
  Restore(REGISTER_CONTINUE);
  next = EvaluateDispatch;
}

void EvaluateSequence() {
  LOG("Evaluate sequence");
  Object unevaluated = GetUnevaluated();
  PrintlnObject(unevaluated);
  SetExpression(FirstExpression(unevaluated));

  if (IsLastExpression(unevaluated)) {
    // Case: 1 expression left to evaluate
    next = EvaluateSequenceLastExpression;
  } else {
    // Case: 2+ expressions left to evaluate
    enum ErrorCode error;
    SAVE(REGISTER_UNEVALUATED, error);
    SAVE(REGISTER_ENVIRONMENT, error);
    SetContinue(EvaluateSequenceContinue);
    next = EvaluateDispatch;
  }
}

void EvaluateIfDecide() {
  LOG("Evaluate if, condition already evaluated");
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
  LOG("Evaluate if");
  enum ErrorCode error;
  SAVE(REGISTER_EXPRESSION, error);
  SAVE(REGISTER_ENVIRONMENT, error);
  SAVE(REGISTER_CONTINUE, error);
  SetContinue(EvaluateIfDecide);
  SetExpression(IfPredicate(GetExpression()));
  next = EvaluateDispatch;
}


void EvaluateAssignment1() {
  LOG("Evaluate assignment, value already evaluated");
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
  LOG("Evaluate assignment");
  SetUnevaluated(AssignmentVariable(GetExpression()));
  enum ErrorCode error;
  SAVE(REGISTER_UNEVALUATED, error);

  SetExpression(AssignmentValue(GetExpression()));
  SAVE(REGISTER_ENVIRONMENT, error);
  SAVE(REGISTER_CONTINUE, error);
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
  DefineVariable();
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
  enum ErrorCode error;
  SAVE(REGISTER_UNEVALUATED, error);

  LOG("Setting, then saving Unevaluated:");
  PrintlnObject(GetUnevaluated());

  SetExpression(DefinitionValue(GetExpression()));

  LOG("Setting the expression:");
  PrintlnObject(GetExpression());

  SAVE(REGISTER_ENVIRONMENT, error);
  SAVE(REGISTER_CONTINUE, error);
  SetContinue(EvaluateDefinition1);

  LOG("Setting continue to %x", EvaluateDefinition1);

  next = EvaluateDispatch;
}

void EvaluateUnknown() {
  LOG_ERROR("Unknown Expression");
  PrintlnObject(GetExpression());
  next = EvaluateError;
}

void EvaluateError() {
  LOG_ERROR("Error occurred.");
  next = 0;
}

Object Second(Object list) { return Car(Cdr(list)); }
Object Third(Object list) { return Car(Cdr(Cdr(list))); }
Object Fourth(Object list) { return Car(Cdr(Cdr(Cdr(list)))); }

Object ApplyPrimitiveProcedure(Object procedure, Object arguments) { 
  LOG("applying primitive function");
  PrintlnObject(procedure);
  PrintlnObject(arguments);
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

Object MakeProcedure(enum ErrorCode *error) {
  Object procedure = AllocateCompoundProcedure(error);
  if (*error) return nil;

  SetProcedureEnvironment(procedure, GetEnvironment());
  SetProcedureParameters(procedure, GetUnevaluated());
  SetProcedureBody(procedure, GetExpression());
  return procedure;
}

void TestEvaluate() {
  InitializeMemory(128);
  InitializeSymbolTable(1);

  PrintlnObject(Evaluate(BoxFixnum(42)));

  enum ErrorCode error;
  ReadObject("'(hello world)", &error);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("(define x 42)", &error);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("(begin 1 2 3)", &error);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("(begin (define x 42) x)", &error);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("(fn (x y z) z)", &error);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("((fn (x y z) z) 1 2 3)", &error);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("((fn () 3))", &error);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("(((fn (z) (fn () z)) 3))", &error);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("+", &error);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("(+ 720 360)", &error);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  ReadObject("(- (+ 720 360) 14)", &error);
  PrintlnObject(GetExpression());
  PrintlnObject(Evaluate(GetExpression()));

  DestroyMemory();
}
