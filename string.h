#ifndef STRING_H
#define STRING_H

#include "memory.h"

Object AllocateString(struct Memory *memory, const char *string);
Object MoveString(struct Memory *memory, u64 ref);
void PrintString(struct Memory *memory, Object object);

#endif
