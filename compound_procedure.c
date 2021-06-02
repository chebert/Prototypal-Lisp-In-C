#include "compound_procedure.h"

#include <assert.h>
#include <stdio.h>

#include "log.h"
#include "memory.h"
#include "tag.h"

Object AllocateCompoundProcedure(enum ErrorCode *error) {
  EnsureEnoughMemory(3, error);
  if (*error) return nil;

  // [ ..., free.. ]
  u64 new_reference = memory.free;
  memory.the_objects[memory.free++] = nil;
  memory.the_objects[memory.free++] = nil;
  memory.the_objects[memory.free++] = nil;
  memory.num_objects_allocated += 3;
  // [ ..., environment, parameters, body, free.. ]
  return BoxCompoundProcedure(new_reference);
}

Object MoveCompoundProcedure(Object procedure) {
  u64 ref = UnboxReference(procedure);
  // New: [ ..., free... ]
  // Old: [ ..., environment, parameters, body, ...] OR
  //      [ ..., <BH new>, ... ]
  u64 new_reference = memory.free;

  LOG(LOG_MEMORY, "moving from %llu in the_objects to %llu in new_objects\n", ref, new_reference);
  Object old_environment = memory.the_objects[ref];
  if (IsBrokenHeart(old_environment)) {
    // The procedure has already been moved. Return the updated reference.
    // Old: [ ..., <BH new>, ... ]
    LOG(LOG_MEMORY, "old_environment is a broken heart pointing to %llu\n", UnboxReference(old_environment));
    return BoxCompoundProcedure(UnboxReference(old_environment));
  }

  LOG(LOG_MEMORY, "Moving from the_objects to new_objects, leaving a broken heart at %llu pointing to %llu\n", ref, new_reference);
  Object moved_procedure = BoxCompoundProcedure(new_reference);
  // Old: [ ..., environment, parameters, body, ... ]
  memory.new_objects[memory.free++] = old_environment; // environment
  memory.new_objects[memory.free++] = memory.the_objects[ref+1]; // parameters
  memory.new_objects[memory.free++] = memory.the_objects[ref+2]; // body
  // New: [ ..., environment, parameters, body, free.. ]
  memory.the_objects[ref] = BoxBrokenHeart(new_reference);
  // Old: [ ...,  <BH new>, ... ]
  return moved_procedure;
}

Object ProcedureEnvironment(Object procedure) {
  assert(IsCompoundProcedure(procedure));
  return memory.the_objects[UnboxReference(procedure)];
}
Object ProcedureParameters(Object procedure) {
  assert(IsCompoundProcedure(procedure));
  return memory.the_objects[UnboxReference(procedure) + 1];
}
Object ProcedureBody(Object procedure) {
  assert(IsCompoundProcedure(procedure));
  return memory.the_objects[UnboxReference(procedure) + 2];
}
void SetProcedureEnvironment(Object procedure, Object environment) {
  assert(IsCompoundProcedure(procedure));
  memory.the_objects[UnboxReference(procedure)] = environment;
}
void SetProcedureParameters(Object procedure, Object parameters) {
  assert(IsCompoundProcedure(procedure));
  memory.the_objects[UnboxReference(procedure) + 1] = parameters;
}
void SetProcedureBody(Object procedure, Object body) {
  assert(IsCompoundProcedure(procedure));
  memory.the_objects[UnboxReference(procedure) + 2] = body;
}

void PrintCompoundProcedure(Object procedure) {
  printf("#procedure(");
  PrintObject(ProcedureEnvironment(procedure));
  printf(" ");
  PrintObject(ProcedureParameters(procedure));
  printf(" ");
  PrintObject(ProcedureBody(procedure));
  printf(")");
}
