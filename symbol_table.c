#include "symbol_table.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "root.h"

// Returns the first object in a list
Object First(struct Memory *memory, Object pair);
// Returns the rest of a list given a pair.
Object Rest(struct Memory *memory, Object pair);

// Returns true if the string object is deep equal to c_string.
b64 IsSymbolEqual(struct Memory *memory, Object symbol, const char *name);

// Returns the index into the symbol_table where name belongs.
u64 GetSymbolListIndex(struct Memory *memory, Object symbol_table, const char *name);
// Returns the symbol or nil.
Object FindSymbolInSymbolList(struct Memory *memory, Object symbol_table, u64 index, const char *name);
// Removes symbol with the given name from the list of symbols.
// Returns the updated symbols list.
Object RemoveSymbolDestructively(struct Memory *memory, Object symbols, const char *name);
// Returns a list of list_b appended to the end of list_a
Object Append(struct Memory *memory, Object list_a, Object list_b);
// Returns list but reversed.
Object Reverse(struct Memory *memory, Object list);

// A Symbol table is a Hashed set, represented as a vector of Symbol Lists
Object MakeSymbolTable(struct Memory *memory, u64 size) {
  return AllocateVector(memory, size);
}

Object FindSymbol(struct Memory *memory, const char *name) {
  Object symbol_table = GetSymbolTable(memory);
  u64 index = GetSymbolListIndex(memory, symbol_table, name);
  return FindSymbolInSymbolList(memory, symbol_table, index, name);
}

Object InternSymbol(struct Memory *memory, const char *name) {
  Object symbol_table = GetSymbolTable(memory);
  u64 index = GetSymbolListIndex(memory, symbol_table, name);
  Object found = FindSymbolInSymbolList(memory, symbol_table, index, name);
  if (found != nil)
    return found;

  // Symbol not found
  // Create a new symbol and add it to the symbol list
  Object symbols = VectorRef(memory, UnboxReference(GetSymbolTable(memory)), index);
  Object new_symbols = AllocatePair(memory, AllocateSymbol(memory, name), symbols);
  VectorSet(memory, UnboxReference(GetSymbolTable(memory)), index, new_symbols);

  return FindSymbol(memory, name);
}

void UninternSymbol(struct Memory *memory, const char *name) {
  Object symbol_table = GetSymbolTable(memory);
  u64 index = GetSymbolListIndex(memory, symbol_table, name);
  Object symbols = VectorRef(memory, UnboxReference(symbol_table), index);

  Object new_symbols = RemoveSymbolDestructively(memory, symbols, name);
  VectorSet(memory, UnboxReference(symbol_table), index, new_symbols);
}


// Helpers

u64 GetSymbolListIndex(struct Memory *memory, Object symbol_table, const char *name) {
  u32 hash = HashString(name);
  s64 length = VectorLength(memory, UnboxReference(symbol_table));
  return hash % length;
}

Object RemoveSymbolDestructively(struct Memory *memory, Object symbols, const char *name) {
  if (symbols == nil) {
    // CASE: ()
    return nil;
  } else if (IsSymbolEqual(memory, First(memory, symbols), name)) {
    // CASE: (symbol . rest)
    return Rest(memory, symbols);
  } else {
    // CASE: element may be somewhere in the rest of the list.
    // symbols := (not-symbol . rest)
    Object result = symbols;

    // prev := (not-symbol . rest)
    // symbols := rest
    Object prev = symbols;
    symbols = Rest(memory, symbols);

    for (; symbols != nil; prev = symbols, symbols = Rest(memory, symbols)) {
      // prev := (x . y . rest)
      // symbols := (y . rest)

      Object symbol = First(memory, symbols);
      if (IsSymbolEqual(memory, symbol, name)) {
        // prev := (x . symbol . rest)
        // symbols := (symbol . rest)
        SetCdr(memory, UnboxReference(prev), Rest(memory, symbols));
        // prev := (x . rest)
        return result;
      }
    }

    return result;
  }
}

Object FindSymbolInSymbolList(struct Memory *memory, Object symbol_table, u64 index, const char *name) {
  for (Object symbols = VectorRef(memory, UnboxReference(symbol_table), index);
      symbols != nil;
      symbols = Rest(memory, symbols)) {
    Object symbol = First(memory, symbols);
    if (IsSymbolEqual(memory, symbol, name))
      return symbol;
  }
  return nil;
}

b64 IsSymbolEqual(struct Memory *memory, Object symbol, const char *name) {
  assert(IsSymbol(symbol));

  u64 symbol_reference = UnboxReference(symbol);
  char *symbol_string = (char*)&memory->the_objects[symbol_reference + 1];

  return strcmp(symbol_string, name) == 0;
}

Object First(struct Memory *memory, Object pair) {
  assert(IsPair(pair));
  return Car(memory, UnboxReference(pair));
}
Object Rest(struct Memory *memory, Object pair) {
  assert(IsPair(pair));
  return Cdr(memory, UnboxReference(pair));
}

// DJB2 algorithm by Dan Bernstein
u32 HashString(const u8 *str) {
  u32 hash = 5381;
  for (u8 c = *str; c; c = *str++)
    hash = hash*33 + c;
  return hash;
}

void TestSymbolTable() {
  assert(HashString("symbol") == 2905944654);

  struct Memory memory = AllocateMemory(128, 13);

  Object symbol = FindSymbol(&memory, "symbol");
  assert(symbol == nil);

  symbol = InternSymbol(&memory, "symbol");
  PrintlnObject(&memory, symbol);

  Object otherSymbol = FindSymbol(&memory, "symbol");
  assert(symbol == otherSymbol);
  otherSymbol = InternSymbol(&memory, "symbol");
  assert(symbol == otherSymbol);

  UninternSymbol(&memory, "symbol");
  assert(FindSymbol(&memory, "symbol") == nil);

  DeallocateMemory(&memory);
  memory = AllocateMemory(128, 1);
  InternSymbol(&memory, "symbol");
  InternSymbol(&memory, "dimple");
  InternSymbol(&memory, "pimple");
  InternSymbol(&memory, "limp-pole");
  assert(FindSymbol(&memory, "symbol") != nil);
  UninternSymbol(&memory, "dimple");
  assert(FindSymbol(&memory, "dimple") == nil);
  assert(FindSymbol(&memory, "symbol") != nil);
  assert(FindSymbol(&memory, "pimple") != nil);
}
