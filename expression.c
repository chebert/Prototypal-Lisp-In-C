#include "expression.h"

#include <assert.h>

#include "pair.h"
#include "symbol_table.h"

#define BEGIN do { 
#define END   } while(0)

b64 IsList(Object list) { return IsNil(list) || IsPair(list); }

b64 IsTaggedList(Object list, const char *tag) {
  return IsPair(list) && FindSymbol(tag) == First(list);
}

b64 IsSelfEvaluating(Object expression) {
  return IsNil(expression)
      || IsTrue(expression)
      || IsFalse(expression)
      || IsFixnum(expression)
      || IsReal64(expression)
      || IsString(expression);
      // NOTE: No syntax for these right now:
      // IsVector(expression)
      // IsByteVector(expression)
}


b64 IsVariable(Object expression)    { return IsSymbol(expression); }
b64 IsApplication(Object expression) { return IsPair(expression); }
b64 IsQuoted(Object expression)      { return IsTaggedList(expression, "quote"); }
b64 IsAssignment(Object expression)  { return IsTaggedList(expression, "set!"); }
b64 IsDefinition(Object expression)  { return IsTaggedList(expression, "define"); }
b64 IsIf(Object expression)          { return IsTaggedList(expression, "if"); }
b64 IsBegin(Object expression)       { return IsTaggedList(expression, "begin"); }
b64 IsLambda(Object expression)      { return IsTaggedList(expression, "fn"); }

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
