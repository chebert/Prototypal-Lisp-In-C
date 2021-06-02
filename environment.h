#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "error.h"
#include "root.h"

// An environment provides a nestable lexical scope.
// It is conceptually a stack of scopes, where each environment scope
// contains a lookup table of symbol->value.

// Attempts to look up the symbol variable in environment, starting with the innermost scope
// and searching outward. If found, returns the value and sets *found to true.
// Otherwise returns nil and sets *found to false.
Object LookupVariableValue(Object variable, Object environment, b64 *found);
// Attempts to look up variable in environment, starting with the innermost scope
// and searching outward. If found, sets the variable's value to value.
// Causes an error if not found.
void SetVariableValue(Object variable, Object value, Object environment, enum ErrorCode *error);

// Creates a new variable in the innermost scope, bound to value.
// If the variable is already in the environment, causes an error.
void DefineVariable(enum ErrorCode *error);
// Creates an environment with a new scope/frame added to the provided environment.
// The symbols in parameters are associated with the values in arguments in the new scope.
void ExtendEnvironment(enum ErrorCode *error);

// Creates the initial environment. Crashes if there isn't enough memory.
void MakeInitialEnvironment(enum ErrorCode *error);

#endif
