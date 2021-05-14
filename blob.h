#ifndef BLOB_H
#define BLOB_H

#include  "memory.h"

// Allocate a blob. Returns the index of the blob.
u64 AllocateBlob(struct Memory *memory, u64 num_bytes);

u64 MoveBlob(struct Memory *memory, u64 ref);

#endif
