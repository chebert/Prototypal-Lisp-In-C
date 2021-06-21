#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "error.h"
#include "tag.h"

b64 IsList(Object list);
Object SetLastCdr(Object list, Object last_pair);

b64 IsTaggedList(Object list, const char *tag);

// Dispatch
b64 IsSelfEvaluating(Object expression);
b64 IsVariable(Object expression);
b64 IsQuoted(Object expression);
b64 IsAssignment(Object expression);
b64 IsDefinition(Object expression);
b64 IsIf(Object expression);
b64 IsLambda(Object expression);
b64 IsBegin(Object expression);
b64 IsApplication(Object expression);

// If
b64 IsTruthy(Object condition);
void ExtractIfPredicate(Object expression, Object *predicate, enum ErrorCode *error);
void ExtractIfAlternatives(Object expression, Object *consequent, Object *alternative, enum ErrorCode *error);

// Application
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

// Assignment/Definition
void ExtractAssignmentOrDefinitionArguments(Object expression, Object *variable, Object *value, 
    enum ErrorCode malformed,
    enum ErrorCode variable_is_non_symbol,
    enum ErrorCode too_many_arguments,
    enum ErrorCode *error);
void ExtractAssignmentArguments(Object expression, Object *variable, Object *value, enum ErrorCode *error);
void ExtractDefinitionArguments(Object expression, Object *variable, Object *value, enum ErrorCode *error);

// Lambda
void ExtractLambdaArguments(Object expression, Object *parameters, Object *body, enum ErrorCode *error);

// Quote
void ExtractQuoted(Object expression, Object *quoted_expression, enum ErrorCode *error);

// Begin
void ExtractBegin(Object expression, Object *sequence, enum ErrorCode *error);

#endif
