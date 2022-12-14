#ifndef BLOB_H
#define BLOB_H

#include "error.h"
#include "tag.h"

// A Blob is an array of bytes, used to implement types like ByteVector and String.
// Memory Layout: [ ..., N, byte0..byte8, ... , byteN..padBytes, ... ]
//  Object boundaries->|  |             | ... |                |
// where 
//   N is the number of bytes in the blob
//   Blob is padded to the nearest Object boundary (|'s emphasize the object boundaries)
//
// The size of a blob in Objects is ceiling(N / 8) + 1

// Allocate a blob. Returns the index of the blob.
u64 AllocateBlob(u64 num_bytes, enum ErrorCode *error);

u64 MoveBlob(u64 ref);

// Returns the number of Objects in a Blob (including header)
u64 NumObjectsPerBlob(u64 bytes_in_blob);

#endif
