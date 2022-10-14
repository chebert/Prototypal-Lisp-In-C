# Prototypal-Lisp-In-C
A prototype of a simple Lisp written in C. Implements a stop and copy GC.
It's doubtful that this is in a useful state, but it may be that the source code is useful as a jumping off point to someone studying languages or GC.

## GC

Implemented as a stop-and-copy garbage collector.

All memory accesses should be performed through the root. This can be found in root.c and root.h.
The garbage collection algorithm is described in memory.h.

```
// Memory is managed using a stop-and-copy garbage collection algorithm.
//
// The root references the live objects in the system. Anything not reachable from the root is considered garbage.
// Memory consists of two regions:
//   the_objects: a mixture the current (live) objects and discarded (garbage) objects in the system.
//   new_objects: during GC, live/reachable objects are moved from the_objects into new_objects.
//      when complete, the new_objects and the_objects are swapped
//
// A Garbage Collection occurs when the system attempts to Allocate and there is not enough memory.
// If there is still not enough memory after GC, the system fails.
// A collection can be invoked early with CollectGarbage();

// Memory Layouts During GC:
//
// Memory layouts in the_objects before and after moving:
// TYPE     | SIZE in Objects        | LAYOUT before Moving                                   | LAYOUT after Moving           |
// ---------|------------------------|--------------------------------------------------------|-------------------------------|
// PRIMITIVE| 1                      |[ ..., primitive, ...]                                  |[ ..., primitive, ...]         |
// REFERENCE| 1                      |[ ..., reference, ...]                                  |[ ..., reference, ...]         |
// PAIR     | 2                      |[ ..., car, cdr, ... ]                                  |[ ..., <BH new>, cdr ]         |
// VECTOR   | nObjects+1             |[ ..., nObjects, object0, object1, .., objectN, ... ]   |[ ..., <BH new>, object0, ... ]|
// BLOB     | ceiling(nBytes, 8) + 1 |[ ..., nBytes, byte0, byte1, .., byteN, pad.., ... ]    |[ ..., <BH new>, byte0, ... ]  |

// Blobs are padded to the nearest Object boundary.
// <BH new>: A Broken Heart points to the newly moved structure in new_objects at index new.
```

Each type has its own semntics for moving. You can learn more about them in pair, vector, blob, byte_vector, etc.


## Evaluation

Tests for evaluation can be found in TestEvaluate in evaluate.c. At some point I thought about naming this language Bertscript, so you may see the code littered with "bert"s here and there.
Evaluation is designed to eliminate tail calls. Therefore control flow for evaluation functions use the macros GOTO, BRANCH, ERROR, CONTINUE, and FINISH.
