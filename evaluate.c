#include "evaluate.h"

#include "log.h"
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

void EvaluateSelfEvaluating(Object expression);
void EvaluateVariable(Object expression);
void EvaluateQuoted(Object expression);
void EvaluateAssignment(Object expression);
void EvaluateDefinition(Object expression);
void EvaluateIf(Object expression);
void EvaluateLambda(Object expression);
void EvaluateSequence(Object expression);
void EvaluateApplication(Object expression);
void EvaluateUnknown(Object expression);

Object LookupVariableValue(Object variable, Object environment, b64 *found);
Object MakeProcedure(Object parameters, Object body, Object environment);
Object LambdaParameters(Object expression);
Object LambdaBody(Object expression);

void Evaluate() {
  // TODO: environment needs
  //   - symbols interned: quote, set!, define, if, fn
  Object expression = GetRegister(REGISTER_EXPRESSION);
  if (IsSelfEvaluating(expression)) {
    EvaluateSelfEvaluating(expression);
  } else if (IsVariable(expression)) {
    EvaluateVariable(expression);
  } else if (IsQuoted(expression)) {
    EvaluateQuoted(expression);
  } else if (IsAssignment(expression)) {
    EvaluateAssignment(expression);
  } else if (IsDefinition(expression)) {
    EvaluateDefinition(expression);
  } else if (IsIf(expression)) {
    EvaluateIf(expression);
  } else if (IsLambda(expression)) {
    EvaluateLambda(expression);
  } else if (IsSequence(expression)) {
    EvaluateSequence(expression);
  } else if (IsApplication(expression)) {
    EvaluateApplication(expression);
  } else {
    EvaluateUnknown(expression);
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

void EvaluateSelfEvaluating(Object expression) {
  SetRegister(REGISTER_VALUE, expression);
}
void EvaluateVariable(Object expression) {
  b64 found = 0;
  Object value = LookupVariableValue(expression, GetRegister(REGISTER_ENVIRONMENT), &found);
  if (!found) {
    LOG_ERROR("Could not find %s in environment", StringCharacterBuffer(expression));
  }
  SetRegister(REGISTER_VALUE, value);
}
void EvaluateQuoted(Object expression) {
  SetRegister(REGISTER_VALUE, Cdr(expression));
}
void EvaluateLambda(Object expression) {
  Object parameters = LambdaParameters(expression);
  SetRegister(REGISTER_UNEVALUATED, parameters);
  Object body = LambdaBody(expression);
  SetRegister(REGISTER_EXPRESSION, body);
  SetRegister(REGISTER_VALUE, MakeProcedure(parameters, body, GetRegister(REGISTER_ENVIRONMENT)));
}

void EvaluateApplication(Object expression) {}
void EvaluateAssignment(Object expression) {}
void EvaluateDefinition(Object expression) {}
void EvaluateIf(Object expression) {}
void EvaluateSequence(Object expression) {}
void EvaluateUnknown(Object expression) {}

Object LookupVariableValue(Object variable, Object environment, b64 *found) {
  return nil;
}

Object MakeProcedure(Object parameters, Object body, Object environment) { return nil; }
Object LambdaParameters(Object expression) { return nil; }
Object LambdaBody(Object expression) { return nil; }

void TestEvaluate() {
}
