#ifndef COMPOUND_PROCEDURE_H
#define COMPOUND_PROCEDURE_H

#include "tag.h"

// A compound procedure type representing 3 associated Objects.
// Memory Layout: [ ..., environment, parameters, body, ... ]
//
// A compound procedure object is a reference to this tuple.

// Allocate a triple representing a compound procedure.
Object AllocateCompoundProcedure();
// Move a compound procedure from the_objects to new_objects
Object MoveCompoundProcedure(Object procedure);

Object ProcedureEnvironment(Object procedure);
Object ProcedureParameters(Object procedure);
Object ProcedureBody(Object procedure);
void SetProcedureEnvironment(Object procedure, Object environment);
void SetProcedureParameters(Object procedure, Object parameters);
void SetProcedureBody(Object procedure, Object body);

void PrintCompoundProcedure(Object procedure);

#endif
