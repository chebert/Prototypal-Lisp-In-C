#include "evaluate.h"

#include <assert.h>

#include "compound_procedure.h"
#include "environment.h"
#include "log.h"
#include "memory.h"
#include "pair.h"
#include "primitives.h"
#include "root.h"
#include "read2.h"
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
b64 IsBegin(Object expression);
b64 IsApplication(Object expression);

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

void EvaluateUnknown();
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

// TODO: Move to expression.h or something
void ExtractAssignmentOrDefinitionArguments(Object expression, Object *variable, Object *value, 
    enum ErrorCode malformed,
    enum ErrorCode variable_is_non_symbol,
    enum ErrorCode too_many_arguments,
    enum ErrorCode *error);
void ExtractAssignmentArguments(Object expression, Object *variable, Object *value, enum ErrorCode *error);
void ExtractLambdaArguments(Object expression, Object *parameters, Object *body, enum ErrorCode *error);
void ExtractDefinitionArguments(Object expression, Object *variable, Object *value, enum ErrorCode *error);
void ExtractIfPredicate(Object expression, Object *predicate, enum ErrorCode *error);
void ExtractIfAlternatives(Object expression, Object *consequent, Object *alternative, enum ErrorCode *error);
void ExtractQuoted(Object expression, Object *quoted_expression, enum ErrorCode *error);
void ExtractBegin(Object expression, Object *sequence, enum ErrorCode *error);

static EvaluateFunction next = 0;
static enum ErrorCode error = NO_ERROR;

#define BEGIN do { 
#define END   } while(0)

// For Evaluate*() functions
#define GOTO(dest)  BEGIN  next = (dest); return;  END
#define BRANCH(test, dest)  BEGIN  if (test) GOTO(dest);  END
#define ERROR(error_code) BEGIN  error = error_code; GOTO(EvaluateError);  END
#define CONTINUE GOTO(GetContinue())

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

b64 IsSelfEvaluating(Object expression) {
  return IsNil(expression)
      || IsTrue(expression)
      || IsFalse(expression)
      || IsFixnum(expression)
      || IsReal64(expression)
      || IsString(expression)
      // NOTE: No syntax for these right now:
      || IsVector(expression)
      || IsByteVector(expression);
}

b64 IsTaggedList(Object list, const char *tag) {
  return IsPair(list) && FindSymbol(tag) == First(list);
}

b64 IsVariable(Object expression)    { return IsSymbol(expression); }
b64 IsApplication(Object expression) { return IsPair(expression); }
b64 IsQuoted(Object expression)      { return IsTaggedList(expression, "quote"); }
b64 IsAssignment(Object expression)  { return IsTaggedList(expression, "set!"); }
b64 IsDefinition(Object expression)  { return IsTaggedList(expression, "define"); }
b64 IsIf(Object expression)          { return IsTaggedList(expression, "if"); }
b64 IsBegin(Object expression)       { return IsTaggedList(expression, "begin"); }
b64 IsLambda(Object expression)      { return IsTaggedList(expression, "fn"); }

void EvaluateSelfEvaluating() {
  LOG(LOG_EVALUATE, "Self evaluating");
  SetValue(GetExpression());
  CONTINUE;
}

void EvaluateVariable() {
  b64 found = 0;
  LOG(LOG_EVALUATE, "Looking up variable value in environment: ");
  LOG_OP(LOG_EVALUATE, PrintlnObject(GetExpression()));
  LOG_OP(LOG_EVALUATE, PrintlnObject(GetEnvironment()));
  Object value = LookupVariableValue(GetExpression(), GetEnvironment(), &found);
  if (!found) {
    LOG_ERROR("Could not find %s in environment", StringCharacterBuffer(GetExpression()));
    ERROR(ERROR_EVALUATE_UNBOUND_VARIABLE);
  }

  SetValue(value);
  CONTINUE;
}

void EvaluateQuoted() {
  LOG(LOG_EVALUATE, "Quoted expression");
  Object quoted_expression;
  CHECK(ExtractQuoted(GetExpression(), &quoted_expression, &error));

  SetValue(quoted_expression);
  CONTINUE;
}


void EvaluateLambda() {
  LOG(LOG_EVALUATE, "Lambda expression");
  Object parameters, body;
  CHECK(ExtractLambdaArguments(GetExpression(), &parameters, &body, &error));

  SetUnevaluated(parameters);
  SetExpression(body);
  Object procedure;
  CHECK(procedure = MakeProcedure(&error));
  SetValue(procedure);
  CONTINUE;
}

void EvaluateApplication() {
  LOG(LOG_EVALUATE, "Application");
  SAVE(REGISTER_CONTINUE);
  SAVE(REGISTER_ENVIRONMENT);
  SetUnevaluated(Operands(GetExpression()));
  SAVE(REGISTER_UNEVALUATED);
  // Evaluate the operator
  SetExpression(Operator(GetExpression()));
  SetContinue(EvaluateApplicationOperands);
  GOTO(EvaluateDispatch);
}

void EvaluateApplicationOperands() {
  // operator has been evaluated.
  Restore(REGISTER_UNEVALUATED);
  Restore(REGISTER_ENVIRONMENT);

  SetProcedure(GetValue());
  SetArgumentList(EmptyArgumentList());

  LOG(LOG_EVALUATE, "Evaluating operands");
  BRANCH(HasNoOperands(GetUnevaluated()), EvaluateApplicationDispatch);

  // CASE: 1 or more operands
  SAVE(REGISTER_PROCEDURE);
  GOTO(EvaluateApplicationOperandLoop);
}
void EvaluateApplicationAccumulateArgument() {
  LOG(LOG_EVALUATE, "Accumulating argument");
  Restore(REGISTER_UNEVALUATED);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_ARGUMENT_LIST);
  CHECK(AdjoinArgument(&error));
  SetUnevaluated(RestOperands(GetUnevaluated()));
  GOTO(EvaluateApplicationOperandLoop);
}

void EvaluateApplicationAccumulateLastArgument() {
  LOG(LOG_EVALUATE, "Accumulating last argument");
  Restore(REGISTER_ARGUMENT_LIST);
  CHECK(AdjoinArgument(&error));
  Restore(REGISTER_PROCEDURE);
  GOTO(EvaluateApplicationDispatch);
}

void EvaluateApplicationOperandLoop() {
  LOG(LOG_EVALUATE, "Evaluating next operand");
  // Evaluate operands
  SAVE(REGISTER_ARGUMENT_LIST);

  SetExpression(FirstOperand(GetUnevaluated()));

  // CASE: Last argument
  if (IsLastOperand(GetUnevaluated())) {
    LOG(LOG_EVALUATE, "last operand");
    SetContinue(EvaluateApplicationAccumulateLastArgument);
    GOTO(EvaluateDispatch);
  } else if (IsPair(GetUnevaluated())) {
    // CASE: 2+ arguments
    LOG(LOG_EVALUATE, "not the last operand");
    SAVE(REGISTER_ENVIRONMENT);
    SAVE(REGISTER_UNEVALUATED);
    SetContinue(EvaluateApplicationAccumulateArgument);
    GOTO(EvaluateDispatch);
  }

  // CASE: arguments are not a list
  ERROR(ERROR_EVALUATE_APPLICATION_DOTTED_LIST);
}

void EvaluateApplicationDispatch() {
  LOG(LOG_EVALUATE, "Evaluate application: dispatch based on primitive/compound procedure");
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
    LOG(LOG_EVALUATE, "Extending the environment");
    CHECK(ExtendEnvironment(&error));

    proc = GetProcedure();
    SetUnevaluated(ProcedureBody(proc));
    GOTO(EvaluateSequence);
  }

  ERROR(ERROR_EVALUATE_UNKNOWN_PROCEDURE_TYPE);
}

void EvaluateBegin() {
  LOG(LOG_EVALUATE, "Evaluate begin");
  Object sequence;
  CHECK(ExtractBegin(GetExpression(), &sequence, &error));

  SetUnevaluated(sequence);
  SAVE(REGISTER_CONTINUE);
  GOTO(EvaluateSequence);
}

void EvaluateSequenceContinue() {
  LOG(LOG_EVALUATE, "Evaluate next expression in sequence");
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_UNEVALUATED);

  SetUnevaluated(RestExpressions(GetUnevaluated()));
  GOTO(EvaluateSequence);
}
void EvaluateSequenceLastExpression() {
  LOG(LOG_EVALUATE, "Evaluate sequence, last expression");
  Restore(REGISTER_CONTINUE);
  GOTO(EvaluateDispatch);
}

void EvaluateSequence() {
  LOG(LOG_EVALUATE, "Evaluate sequence");
  Object unevaluated = GetUnevaluated();
  LOG_OP(LOG_EVALUATE, PrintlnObject(unevaluated));

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
  LOG(LOG_EVALUATE, "Evaluate if, condition already evaluated");
  Restore(REGISTER_CONTINUE);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_EXPRESSION);

  Object consequent, alternative;
  CHECK(ExtractIfAlternatives(GetExpression(), &consequent, &alternative, &error));

  SetExpression(IsTruthy(GetValue()) ? consequent : alternative);
  GOTO(EvaluateDispatch);
}

void EvaluateIf() {
  LOG(LOG_EVALUATE, "Evaluate if");
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
  LOG(LOG_EVALUATE, "Evaluate assignment, value already evaluated");
  Restore(REGISTER_CONTINUE);
  Restore(REGISTER_ENVIRONMENT);
  Restore(REGISTER_UNEVALUATED);

  CHECK(SetVariableValue(GetUnevaluated(), GetValue(), GetEnvironment(), &error));
  // Return the symbol 'ok as the result of an assignment
  SetValue(FindSymbol("ok"));
  CONTINUE;
}


void EvaluateAssignment() {
  LOG(LOG_EVALUATE, "Evaluate assignment");

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
  SetValue(GetUnevaluated());
  CONTINUE;
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
  GOTO(0);
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

// if test fails, set the *error to the error_code and return
#define ENSURE(test, error_code, error) BEGIN  if (!(test)) { *error = error_code; return; }  END

void ExtractLambdaArguments(Object expression, Object *parameters, Object *body, enum ErrorCode *error) {
  expression = Rest(expression);
  // (lambda ...)

  ENSURE(IsPair(expression), ERROR_EVALUATE_LAMBDA_MALFORMED, error);
  // (lambda parameters ...)
  *parameters = First(expression);
  ENSURE(IsList(*parameters), ERROR_EVALUATE_LAMBDA_PARAMETERS_SHOULD_BE_LIST, error);

  // (lambda parameters body)
  *body = Rest(expression);
  ENSURE(!IsNil(*body), ERROR_EVALUATE_LAMBDA_BODY_SHOULD_BE_NON_EMPTY, error);
  ENSURE(IsPair(*body), ERROR_EVALUATE_LAMBDA_BODY_MALFORMED, error);
}

void ExtractIfPredicate(Object expression, Object *predicate, enum ErrorCode *error) {
  expression = Rest(expression);
  ENSURE(IsPair(expression), ERROR_EVALUATE_IF_MALFORMED, error);

  *predicate = First(expression);
}

void ExtractIfAlternatives(Object expression, Object *consequent, Object *alternative, enum ErrorCode *error) {
  expression = Rest(Rest(expression));
  ENSURE(IsPair(expression), ERROR_EVALUATE_IF_MALFORMED, error);

  // (if predicate consequent ...)
  *consequent = First(expression);
  expression = Rest(expression);
  ENSURE(IsPair(expression), ERROR_EVALUATE_IF_MALFORMED, error);

  // (if predicate consequent alternative ...)
  *alternative = First(expression);
  ENSURE(IsNil(expression), ERROR_EVALUATE_IF_TOO_MANY_ARGUMENTS, error);
}

void ExtractAssignmentOrDefinitionArguments(Object expression, Object *variable, Object *value, 
    enum ErrorCode malformed,
    enum ErrorCode variable_is_non_symbol,
    enum ErrorCode too_many_arguments,
    enum ErrorCode *error) {
  expression = Rest(expression);
  ENSURE(IsPair(expression), malformed, error);

  // (set! variable ...)
  *variable = First(expression);
  ENSURE(IsSymbol(*variable), variable_is_non_symbol, error);

  expression = Rest(expression);
  ENSURE(IsPair(expression), malformed, error); 

  // (set! variable value ...)
  *value = First(expression);
  expression = Rest(expression);
  ENSURE(IsNil(expression), too_many_arguments, error);
}

void ExtractQuoted(Object expression, Object *quoted_expression, enum ErrorCode *error) {
  expression = Rest(expression);
  ENSURE(IsPair(expression), ERROR_EVALUATE_QUOTE_MALFORMED, error);

  ENSURE(IsNil(Rest(expression)), ERROR_EVALUATE_QUOTE_TOO_MANY_ARGUMENTS, error);
  *quoted_expression = First(expression);
}
void ExtractBegin(Object expression, Object *sequence, enum ErrorCode *error) {
  expression = Rest(expression);
  ENSURE(!IsNil(expression), ERROR_EVALUATE_BEGIN_EMPTY, error);
  ENSURE(IsPair(expression), ERROR_EVALUATE_BEGIN_MALFORMED, error);
  *sequence = expression;
}
#undef ENSURE

void ExtractAssignmentArguments(Object expression, Object *variable, Object *value, enum ErrorCode *error) {
  ExtractAssignmentOrDefinitionArguments(expression, variable, value,
      ERROR_EVALUATE_SET_MALFORMED,
      ERROR_EVALUATE_SET_NON_SYMBOL,
      ERROR_EVALUATE_SET_TOO_MANY_ARGUMENTS,
      error);
}

void ExtractDefinitionArguments(Object expression, Object *variable, Object *value, enum ErrorCode *error) {
  ExtractAssignmentOrDefinitionArguments(expression, variable, value,
      ERROR_EVALUATE_DEFINE_MALFORMED,
      ERROR_EVALUATE_DEFINE_NON_SYMBOL,
      ERROR_EVALUATE_DEFINE_TOO_MANY_ARGUMENTS,
      error);
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

static void ReadObject(const u8 *source, enum ErrorCode *error) {
  SetReadSourceFromString(source, error);
  SetRegister(REGISTER_READ_SOURCE_POSITION, 0);
  if (*error) return;
  ReadFromSource(error);
}


void TestEvaluate() {
  InitializeMemory(1024, &error);
  InitializeSymbolTable(1, &error);

  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(BoxFixnum(42))));

#define BERT(bertscript...) #bertscript

  ReadObject("'(hello world)", &error);
  /*
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(define x 42)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(begin 1 2 3)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(begin (define x 42) x)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(fn (x y z) z)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("((fn (x y z) z) 1 2 3)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("((fn () 3))", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(((fn (z) (fn () z)) 3))", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("+:binary", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(+:binary 720 360)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(-:binary (+:binary 720 360) 14)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject(BERT((-:binary "hello" 14)), &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(-:unary 14)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(+:binary 14 3e3)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(/:binary 14 0)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(/:binary 14 7)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(remainder 3 4)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject(BERT(
        (begin
         (define pair
          (fn (left right)
           ((fn (pair)
             (set-pair-left! pair left)
             (set-pair-right! pair right)
             pair)
            (allocate-pair))))
         (pair 3 4))), &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(list 1 2 3 4 5)", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject("(evaluate '(+:binary 1 2))", &error);
  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  ReadObject(BERT(
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

  LOG_OP(LOG_TEST, PrintlnObject(GetExpression()));
  LOG_OP(LOG_TEST, PrintlnObject(EvaluateInAFreshEnvironment(GetExpression())));

  */
  DestroyMemory();
}
