#include "evaluate.h"

#include <assert.h>

#include "compound_procedure.h"
#include "environment.h"
#include "expression.h"
#include "log.h"
#include "memory.h"
#include "pair.h"
#include "primitives.h"
#include "root.h"
#include "read.h"
#include "string.h"
#include "symbol_table.h"

Object EvaluateInAFreshEnvironment(Object expression);

// EvaluateFunctions
// REGISTER_EXPRESSION (IN) is evaluated
// REGISTER_VALUE (OUT) holds the value
// REGISTER_CONTINUE (IN) where to continue execution when complete.
// Stack (...)
void EvaluateDispatch();
void EvaluateSelfEvaluating();
void EvaluateVariable();
void EvaluateQuoted();
void EvaluateAssignment();
void EvaluateDefinition();
void EvaluateIf();
void EvaluateLambda();
void EvaluateBegin();
void EvaluateApplication();

// REGISTER_UNEVALUATED (IN) holds a list of expressions to evaluate
void EvaluateSequence();
// Stack (continue ...)
void EvaluateSequenceLastExpression();
// Stack (environment unevaluated ...) 
//   environment: environment of the sequence
//   unevaluated: remaining expressions in sequence
void EvaluateSequenceContinue();

// REGISTER_VALUE (IN) holds the evaluated operator
// Stack (unevaluated environment ...)
//   unevaluted: holds the unevaluated operands
//   environemnt: holds the environment of the application
void EvaluateApplicationOperands();

// REGISTER_PROCEDURE (IN) holds the procedure
// Stack (continue ...) 
//   continue: holds where to resume execution when the application is complete.
void EvaluateApplicationDispatch();

// REGISTER_UNEVALUATED (IN) The unevaluated arguments.
void EvaluateApplicationOperandLoop();

// REGISTER_UNEVALUATED (IN/OUT) The unevaluated arguments.
// REGISTER_ARGUMENT_LIST (IN/OUT) The evaluated arguments.
// Stack (unevaluated environment argument_list ...)
//   unevaluated: the unevaluated arguments
//   envrionment: environment of the application
//   argument_list: the evaluated arguments
void EvaluateApplicationAccumulateArgument();

// REGISTER_ARGUMENT_LIST (IN/OUT) The evaluated arguments.
// Stack (argument_list procedure ...)
//   argument_list: the evaluated arguments
//   procedure: the evaluated operator
void EvaluateApplicationAccumulateLastArgument();

// Stack (continue environment expression ...)
//   continue: where to resume when if expression is fully evaluated
//   environment: environment of the if expression
//   expression: (if predicate consequent alternative)
void EvaluateIfDecide();
void EvaluateAssignment1();
void EvaluateDefinition1();

void EvaluateUnboundVariable();

void EvaluateUnknown();
void EvaluateError();

Object MakeProcedure(enum ErrorCode *error);

// Procedures
Object ApplyPrimitiveProcedure(Object procedure, Object arguments, enum ErrorCode *error);

// Application
void AdjoinArgument(enum ErrorCode *error);

static EvaluateFunction next = 0;
static enum ErrorCode error = NO_ERROR;

#define BEGIN do { 
#define END   } while(0)

// For Evaluate*() functions
#define GOTO(dest)  BEGIN  next = (dest); return;  END
#define BRANCH(test, dest)  BEGIN  if (test) GOTO(dest);  END
#define ERROR(error_code) BEGIN  error = error_code; GOTO(EvaluateError);  END
#define CONTINUE GOTO(GetContinue())
#define FINISH(value) BEGIN  SetValue(value); CONTINUE;  END

#define CHECK(op)  BEGIN  (op); if (error) { GOTO(EvaluateError); }  END
#define SAVE(reg)  BEGIN  CHECK(Save((reg), &error));  END

void DefinePrimitive(const u8 *name, PrimitiveFunction function) {
  enum ErrorCode error;
  SetUnevaluated(InternSymbol(name, &error));
  assert(!error);
  SetValue(BoxPrimitiveProcedure(function));
  DefineVariable(&error);
  assert(!error);
}

// TODO: take & return error code
Object Evaluate(Object expression) {
  SetExpression(expression);
  // Set continue to quit when evaluation finishes.
  SetContinue(0);

  // Start the evaluation by evaluating the provided expression.
  next = EvaluateDispatch;

  // Run the evaluation loop until quit.
  while (next) next();

  // Return the evaluated expression.
  return GetValue();
}

Object EvaluateInAFreshEnvironment(Object expression) {
  error = NO_ERROR;
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

  return Evaluate(GetExpression());
}

void EvaluateDispatch() {
  Object expression = GetExpression();
  LOG(LOG_EVALUATE, "Evaluating expression:");
  LOG_OP(LOG_EVALUATE, PrintlnObject(expression));

  BRANCH(IsSelfEvaluating(expression), EvaluateSelfEvaluating);
  BRANCH(IsVariable(expression),       EvaluateVariable);
  BRANCH(IsQuoted(expression),         EvaluateQuoted);
  BRANCH(IsAssignment(expression),     EvaluateAssignment);
  BRANCH(IsDefinition(expression),     EvaluateDefinition);
  BRANCH(IsIf(expression),             EvaluateIf);
  BRANCH(IsLambda(expression),         EvaluateLambda);
  BRANCH(IsBegin(expression),          EvaluateBegin);
  BRANCH(IsApplication(expression),    EvaluateApplication);
  GOTO(EvaluateUnknown);
}

void EvaluateSelfEvaluating() {
  FINISH(GetExpression());
}

void EvaluateVariable() {
  b64 found = 0;
  Object value = LookupVariableValue(GetExpression(), GetEnvironment(), &found);
  BRANCH(!found, EvaluateUnboundVariable);
  FINISH(value);
}

void EvaluateUnboundVariable() {
  LOG_ERROR("Could not find %s in environment", StringCharacterBuffer(GetExpression()));
  ERROR(ERROR_EVALUATE_UNBOUND_VARIABLE);
}

void EvaluateQuoted() {
  Object quoted_expression;
  CHECK(ExtractQuoted(GetExpression(), &quoted_expression, &error));
  FINISH(quoted_expression);
}


void EvaluateLambda() {
  Object parameters, body;
  CHECK(ExtractLambdaArguments(GetExpression(), &parameters, &body, &error));

  SetUnevaluated(parameters);
  SetExpression(body);
  
  Object procedure;
  CHECK(procedure = AllocateCompoundProcedure(error));

  SetProcedureEnvironment(procedure, GetEnvironment());
  SetProcedureParameters(procedure, GetUnevaluated());
  SetProcedureBody(procedure, GetExpression());

  FINISH(procedure);
}

void EvaluateApplication() {
  SAVE(REGISTER_CONTINUE);
  SAVE(REGISTER_ENVIRONMENT);
  SetUnevaluated(Operands(GetExpression()));
  SAVE(REGISTER_UNEVALUATED);
  // First: Evaluate the operator
  SetExpression(Operator(GetExpression()));
  // Continue by evaluating the operands
  SetContinue(EvaluateApplicationOperands);
  // Evaluate the operator
  GOTO(EvaluateDispatch);
}

void EvaluateApplicationOperands() {
  // FROM: EvaluateApplication
  // operator has been evaluated.
  Restore(REGISTER_UNEVALUATED);
  Restore(REGISTER_ENVIRONMENT);

  // Save the evaluated procedure.
  SetProcedure(GetValue());
  SetArgumentList(EmptyArgumentList());

  // CASE: no operands
  BRANCH(HasNoOperands(GetUnevaluated()), EvaluateApplicationDispatch);
  // CASE: 1 or more operands
  SAVE(REGISTER_PROCEDURE);
  GOTO(EvaluateApplicationOperandLoop);
}

void EvaluateApplicationAccumulateArgument() {
  // FROM: EvaluateApplicationOperandLoop
  Restore(REGISTER_UNEVALUATED);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_ARGUMENT_LIST);
  CHECK(AdjoinArgument(&error));
  SetUnevaluated(RestOperands(GetUnevaluated()));
  GOTO(EvaluateApplicationOperandLoop);
}

void EvaluateApplicationAccumulateLastArgument() {
  Restore(REGISTER_ARGUMENT_LIST);
  CHECK(AdjoinArgument(&error));
  Restore(REGISTER_PROCEDURE);
  GOTO(EvaluateApplicationDispatch);
}

void EvaluateApplicationOperandLoop() {
  // FROM: EvaluateApplicationAccumulateArgument or EvaluateApplicationOperands
  // Evaluate operands
  SAVE(REGISTER_ARGUMENT_LIST);

  SetExpression(FirstOperand(GetUnevaluated()));

  // CASE: Last argument
  if (IsLastOperand(GetUnevaluated())) {
    SetContinue(EvaluateApplicationAccumulateLastArgument);
    GOTO(EvaluateDispatch);
  } else if (IsPair(GetUnevaluated())) {
    // CASE: 2+ arguments
    SAVE(REGISTER_ENVIRONMENT);
    SAVE(REGISTER_UNEVALUATED);
    SetContinue(EvaluateApplicationAccumulateArgument);
    GOTO(EvaluateDispatch);
  }

  // CASE: arguments are not a list
  ERROR(ERROR_EVALUATE_APPLICATION_DOTTED_LIST);
}

void EvaluateApplicationDispatch() {
  Object proc = GetProcedure();
  if (IsPrimitiveProcedure(proc)) {
    // Primitive-procedure application
    CHECK(SetValue(ApplyPrimitiveProcedure(proc, GetArgumentList(), &error)));
    Restore(REGISTER_CONTINUE);
    CONTINUE;
  } else if (IsCompoundProcedure(proc)) {
    // Compound-procedure application
    SetUnevaluated(ProcedureParameters(proc));
    SetEnvironment(ProcedureEnvironment(proc));
    CHECK(ExtendEnvironment(&error));

    proc = GetProcedure();
    SetUnevaluated(ProcedureBody(proc));
    GOTO(EvaluateSequence);
  }

  ERROR(ERROR_EVALUATE_UNKNOWN_PROCEDURE_TYPE);
}

void EvaluateBegin() {
  Object sequence;
  CHECK(ExtractBegin(GetExpression(), &sequence, &error));

  SetUnevaluated(sequence);
  SAVE(REGISTER_CONTINUE);
  GOTO(EvaluateSequence);
}

void EvaluateSequenceContinue() {
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_UNEVALUATED);

  SetUnevaluated(RestExpressions(GetUnevaluated()));
  GOTO(EvaluateSequence);
}
void EvaluateSequenceLastExpression() {
  Restore(REGISTER_CONTINUE);
  GOTO(EvaluateDispatch);
}

void EvaluateSequence() {
  Object unevaluated = GetUnevaluated();
  if (IsPair(unevaluated)) {
    SetExpression(FirstExpression(unevaluated));

    // Case: 1 expression left to evaluate
    BRANCH(IsLastExpression(unevaluated), EvaluateSequenceLastExpression);

    // Case: 2+ expressions left to evaluate
    SAVE(REGISTER_UNEVALUATED);
    SAVE(REGISTER_ENVIRONMENT);
    SetContinue(EvaluateSequenceContinue);
    GOTO(EvaluateDispatch);
  }

  // Case: 0 expressions in sequence
  ERROR(ERROR_EVALUATE_SEQUENCE_EMPTY);
}


void EvaluateIfDecide() {
  Restore(REGISTER_CONTINUE);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_EXPRESSION);

  Object consequent, alternative;
  CHECK(ExtractIfAlternatives(GetExpression(), &consequent, &alternative, &error));

  SetExpression(IsTruthy(GetValue()) ? consequent : alternative);
  GOTO(EvaluateDispatch);
}

void EvaluateIf() {
  SAVE(REGISTER_EXPRESSION);
  SAVE(REGISTER_ENVIRONMENT);
  SAVE(REGISTER_CONTINUE);
  SetContinue(EvaluateIfDecide);

  Object predicate;
  CHECK(ExtractIfPredicate(GetExpression(), &predicate, &error));

  SetExpression(predicate);
  GOTO(EvaluateDispatch);
}


void EvaluateAssignment1() {
  Restore(REGISTER_CONTINUE);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_UNEVALUATED);

  CHECK(SetVariableValue(GetUnevaluated(), GetValue(), GetEnvironment(), &error));
  // Return the symbol 'ok as the result of an assignment
  FINISH(FindSymbol("ok"));
}


void EvaluateAssignment() {
  Object variable, value;
  CHECK(ExtractAssignmentArguments(GetExpression(), &variable, &value, &error));

  SetUnevaluated(variable);
  SAVE(REGISTER_UNEVALUATED);
  SetExpression(value);
  SAVE(REGISTER_ENVIRONMENT);
  SAVE(REGISTER_CONTINUE);
  SetContinue(EvaluateAssignment1);
  GOTO(EvaluateDispatch);
}

void EvaluateDefinition1() {
  Restore(REGISTER_CONTINUE);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_UNEVALUATED);
  CHECK(DefineVariable(&error));
  // Return the symbol name as the result of the definition.
  FINISH(GetUnevaluated());
}

void EvaluateDefinition() {
  // (define name value)
  Object variable, value;
  CHECK(ExtractDefinitionArguments(GetExpression(), &variable, &value, &error));

  // (define name value)
  SetUnevaluated(variable);
  SAVE(REGISTER_UNEVALUATED);
  SetExpression(value);
  SAVE(REGISTER_ENVIRONMENT);
  SAVE(REGISTER_CONTINUE);
  SetContinue(EvaluateDefinition1);
  GOTO(EvaluateDispatch);
}

void EvaluateUnknown() {
  LOG_ERROR("Unknown Expression");
  LOG_OP(LOG_EVALUATE, PrintlnObject(GetExpression()));

  ERROR(ERROR_EVALUATE_UNKNOWN_EXPRESSION);
}

void EvaluateError() {
  LOG_ERROR("%s", ErrorCodeString(error));
  error = NO_ERROR;
  GOTO(0);
}

Object ApplyPrimitiveProcedure(Object procedure, Object arguments, enum ErrorCode *error) { 
  PrimitiveFunction function = UnboxPrimitiveProcedure(procedure);
  return function(arguments, error);
}

void AdjoinArgument(enum ErrorCode *error) {
  Object last_pair = AllocatePair(error);
  if (*error) return;

  SetCar(last_pair, GetValue());
  SetArgumentList(SetLastCdr(GetArgumentList(), last_pair));
}

static Object ReadObject(const u8 *source, enum ErrorCode *error) {
  *error = NO_ERROR;
  Object string = AllocateString(source, error);
  if (*error) return nil;
  s64 position = 0;
  return ReadFromString(string, &position, error);
}


void TestEvaluate() {
  InitializeMemory(1024, &error);
  InitializeSymbolTable(1, &error);

  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(BoxFixnum(42))));

#define BERT(bertscript...) #bertscript

  Object expression = ReadObject("'(hello world)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(define x 42)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(begin 1 2 3)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(begin (define x 42) x)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(fn (x y z) z)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("((fn (x y z) z) 1 2 3)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("((fn () 3))", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(((fn (z) (fn () z)) 3))", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("+:binary", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(+:binary 720 360)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(-:binary (+:binary 720 360) 14)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject(BERT((-:binary "hello" 14)), &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(-:unary 14)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(+:binary 14 3e3)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(/:binary 14 0)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(/:binary 14 7)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(remainder 3 4)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject(BERT(
        (begin
         (define pair
          (fn (left right)
           ((fn (pair)
             (set-pair-left! pair left)
             (set-pair-right! pair right)
             pair)
            (allocate-pair))))
         (pair 3 4))), &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(list 1 2 3 4 5)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject("(evaluate '(+:binary 1 2))", &error);
  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));

  expression = ReadObject(BERT(
        (begin
         (define read-entire-file
          (fn (filename)
           ((fn (file)
             ((fn (buffer)
               (copy-file-contents! file buffer)
               (close-file! file)
               (byte-vector->string buffer))
              (allocate-byte-vector (file-length file))))
            (open-binary-file-for-reading! filename))))
         (read-entire-file "evaluate.h"))), &error);

  LOG_OP(LOG_TEST, PrintlnObject(expression));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(expression)));
  DestroyMemory();
}
