#include "evaluate.h"

#include <assert.h>

#include "compound_procedure.h"
#include "environment.h"
#include "log.h"
#include "memory.h"
#include "pair.h"
#include "primitives.h"
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

b64 IsList(Object list);
Object MakeProcedure(enum ErrorCode *error);

// Procedures
Object ApplyPrimitiveProcedure(Object procedure, Object arguments, enum ErrorCode *error);

// If
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
Object FirstExpression(Object sequence);
Object RestExpressions(Object sequence);
b64 IsLastExpression(Object sequence);

#define CHECK(error) \
  do { if (error) { next = EvaluateError; return; } } while(0)

#define SAVE_AND_CHECK(reg, error) \
  do { Save(reg, &error); CHECK(error); } while(0)

static EvaluateFunction next = 0;
static enum ErrorCode error = NO_ERROR;

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
#define X(name, function) DefinePrimitive(name, function);
  PRIMITIVES
#undef X

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
  if (found) {
    SetValue(value);
    next = GetContinue();
  } else {
    LOG_ERROR("Could not find %s in environment", StringCharacterBuffer(GetExpression()));
    error = ERROR_EVALUATE_UNBOUND_VARIABLE;
    next = EvaluateError;
  }
}

void EvaluateQuoted() {
  LOG(LOG_EVALUATE, "Quoted expression");
  Object expression = Rest(GetExpression());
  if (IsPair(expression)) {
    // (quote object ...)
    if (IsNil(Rest(expression))) {
      // (quote object)
      Object quoted_object = First(expression);
      SetValue(quoted_object);
      next = GetContinue();
    } else {
      // (quote object junk...)
      error = ERROR_EVALUATE_QUOTE_TOO_MANY_ARGUMENTS;
      next = EvaluateError;
    }
  } else {
    // (quote . object)
    error = ERROR_EVALUATE_QUOTE_MALFORMED;
    next = EvaluateError;
  }
}
void EvaluateLambda() {
  LOG(LOG_EVALUATE, "Lambda expression");
  Object expression = Rest(GetExpression());
  // (lambda ...)

  if (IsPair(expression)) {
    // (lambda parameters ...)
    Object parameters = First(expression);
    if (IsList(parameters)) {
      // (lambda parameters body)
      Object body = Rest(expression);
      if (IsPair(body)) {
        SetUnevaluated(parameters);
        SetExpression(body);
        LOG(LOG_EVALUATE, "Lambda body: ");
        LOG_OP(LOG_EVALUATE, PrintlnObject(body));
        SetValue(MakeProcedure(&error));
        CHECK(error);
        next = GetContinue();
      } else if (IsNil(body)) {
        // (lambda parameters)
        error = ERROR_EVALUATE_LAMBDA_BODY_SHOULD_BE_NON_EMPTY;
        next = EvaluateError;
      } else {
        // (lambda parameters . non-pair)
        Object body = Rest(expression);
        error = ERROR_EVALUATE_LAMBDA_BODY_MALFORMED;
        next = EvaluateError;
      }
    } else {
      // (lambda non-list)
      error = ERROR_EVALUATE_LAMBDA_PARAMETERS_SHOULD_BE_LIST;
      next = EvaluateError;
    }
  } else {
    // (lambda . parameters)
    error = ERROR_EVALUATE_LAMBDA_MALFORMED;
    next = EvaluateError;
  }
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
  assert(IsList(list));
  assert(IsPair(last_pair));
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
  } else if (IsPair(GetUnevaluated())) {
    // CASE: 2+ arguments
    LOG(LOG_EVALUATE, "not the last operand");
    SAVE_AND_CHECK(REGISTER_ENVIRONMENT, error);
    SAVE_AND_CHECK(REGISTER_UNEVALUATED, error);
    SetContinue(EvaluateApplicationAccumulateArgument);
    next = EvaluateDispatch;
  } else {
    // CASE: arguments are not a list
    error = ERROR_EVALUATE_APPLICATION_DOTTED_LIST;
    next = EvaluateError;
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
  Object expression = Rest(GetExpression());
  if (IsPair(expression)) {
    // (begin expressions...)
    SetUnevaluated(expression);
    SAVE_AND_CHECK(REGISTER_CONTINUE, error);
    next = EvaluateSequence;
  } else if (IsNil(expression)) {
    // (begin)
    error = ERROR_EVALUATE_BEGIN_EMPTY;
    next = EvaluateError;
  } else {
    // (begin . junk)
    error = ERROR_EVALUATE_BEGIN_MALFORMED;
    next = EvaluateError;
  }
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

  if (IsPair(unevaluated)) {
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
  } else {
    // Case: 0 expressions in sequence
    error = ERROR_EVALUATE_SEQUENCE_EMPTY;
    next = EvaluateError;
  }
}

void EvaluateIfDecide() {
  LOG(LOG_EVALUATE, "Evaluate if, condition already evaluated");
  Restore(REGISTER_CONTINUE);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_EXPRESSION);

  // IfExpression := (if predicate consequent alternative)
  Object expression = Rest(Rest(GetExpression()));
  if (IsPair(expression)) {
    // (if predicate consequent ...)
    Object consequent = First(expression);
    expression = Rest(expression);
    if (IsPair(expression)) {
      // (if predicate consequent alternative ...)
      Object alternative = First(expression);
      if (IsNil(Rest(expression))) {
        // (if predicate consequent alternative)
        // expression := (alternative)
        if (IsTruthy(GetValue())) {
          SetExpression(consequent);
        } else {
          SetExpression(alternative);
        }
        next = EvaluateDispatch;
      } else {
        // (if predicate consequent alternative junk ...)
        error = ERROR_EVALUATE_IF_TOO_MANY_ARGUMENTS;
        next = EvaluateError;
      }
    } else {
      // (if predicate expression . alternative)
      error = ERROR_EVALUATE_IF_MALFORMED;
      next = EvaluateError;
    }
  } else {
    // (if predicate . expression)
    error = ERROR_EVALUATE_IF_MALFORMED;
    next = EvaluateError;
  }
}

void EvaluateIf() {
  LOG(LOG_EVALUATE, "Evaluate if");
  SAVE_AND_CHECK(REGISTER_EXPRESSION, error);
  SAVE_AND_CHECK(REGISTER_ENVIRONMENT, error);
  SAVE_AND_CHECK(REGISTER_CONTINUE, error);
  SetContinue(EvaluateIfDecide);

  Object expression = Rest(GetExpression());
  if (IsPair(expression)) {
    // (if predicate ...)
    SetExpression(First(expression));
    next = EvaluateDispatch;
  } else {
    error = ERROR_EVALUATE_IF_MALFORMED;
    next = EvaluateError;
  }
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

  Object expression = Rest(GetExpression());
  if (IsPair(expression)) {
    // (set! variable ...)
    Object variable = First(expression);
    if (IsSymbol(variable)) {
      expression = Rest(expression);
      if (IsPair(expression)) {
        // (set! variable value ...)
        Object value = First(expression);
        expression = Rest(expression);
        if (IsNil(expression)) {
          // (set! variable value)
          SetUnevaluated(variable);
          SAVE_AND_CHECK(REGISTER_UNEVALUATED, error);
          SetExpression(value);
          SAVE_AND_CHECK(REGISTER_ENVIRONMENT, error);
          SAVE_AND_CHECK(REGISTER_CONTINUE, error);
          SetContinue(EvaluateAssignment1);
          next = EvaluateDispatch;
        } else {
          // (set! variable value junk...)
          error = ERROR_EVALUATE_SET_TOO_MANY_ARGUMENTS;
          next = EvaluateError;
        }
      } else {
        // (set! variable . value)
        error = ERROR_EVALUATE_SET_MALFORMED;
        next = EvaluateError;
      }
    } else {
      // (set! non-variable ...)
      error = ERROR_EVALUATE_SET_NON_SYMBOL;
      next = EvaluateError;
    }
  } else {
    // (set! . variable)
    error = ERROR_EVALUATE_SET_MALFORMED;
    next = EvaluateError;
  }
}

void EvaluateDefinition1() {
  Restore(REGISTER_CONTINUE);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_UNEVALUATED);
  DefineVariable(&error);
  CHECK(error);
  // Return the symbol name as the result of the definition.
  SetValue(GetUnevaluated());
  next = GetContinue();
}

void EvaluateDefinition() {
  // (define name value)

  Object expression = Rest(GetExpression());
  // (define ...)

  if (IsPair(expression)) {
    // (define name ...)
    Object variable = First(expression);
    if (IsSymbol(variable)) {
      expression = Rest(expression);
      if (IsPair(expression)) {
        // (define name value ...)
        Object value = First(expression);
        expression = Rest(expression);
        if (IsNil(expression)) {
          // (define name value)
          SetUnevaluated(variable);
          SAVE_AND_CHECK(REGISTER_UNEVALUATED, error);
          SetExpression(value);
          SAVE_AND_CHECK(REGISTER_ENVIRONMENT, error);
          SAVE_AND_CHECK(REGISTER_CONTINUE, error);
          SetContinue(EvaluateDefinition1);
          next = EvaluateDispatch;
        } else {
          // (define name value junk...)
          error = ERROR_EVALUATE_DEFINE_TOO_MANY_ARGUMENTS;
          next = EvaluateError;
        }
      } else {
        // (define name . value)
        error = ERROR_EVALUATE_DEFINE_MALFORMED;
        next = EvaluateError;
      }
    } else {
      // (define non-symbol ...)
      error = ERROR_EVALUATE_DEFINE_NON_SYMBOL;
      next = EvaluateError;
    }
  } else {
    // (define . name)
    error = ERROR_EVALUATE_DEFINE_MALFORMED;
    next = EvaluateError;
  }
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

Object ApplyPrimitiveProcedure(Object procedure, Object arguments, enum ErrorCode *error) { 
  LOG(LOG_EVALUATE, "applying primitive function");
  LOG_OP(LOG_EVALUATE, PrintlnObject(procedure));
  LOG_OP(LOG_EVALUATE, PrintlnObject(arguments));
  PrimitiveFunction function = UnboxPrimitiveProcedure(procedure);
  return function(arguments, error);
}

// If: (if condition consequent alternative)
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
Object FirstExpression(Object sequence)  { return First(sequence); }
Object RestExpressions(Object sequence)  { return Rest(sequence); }
b64    IsLastExpression(Object sequence) { return IsNil(RestExpressions(sequence)); }

b64 IsList(Object list) { return IsNil(list) || IsPair(list); }

Object MakeProcedure(enum ErrorCode *error) {
  Object procedure = AllocateCompoundProcedure(error);
  if (*error) return nil;

  SetProcedureEnvironment(procedure, GetEnvironment());
  SetProcedureParameters(procedure, GetUnevaluated());
  SetProcedureBody(procedure, GetExpression());
  return procedure;
}

void TestEvaluate() {
  InitializeMemory(256, &error);
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

  ReadObject("+:binary", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(+:binary 720 360)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(-:binary (+:binary 720 360) 14)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(-:binary \"hello\" 14)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(-:unary 14)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  ReadObject("(+:binary 14 3e3)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(Evaluate(GetExpression())));

  DestroyMemory();
}
