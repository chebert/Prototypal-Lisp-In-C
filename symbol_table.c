#include "symbol_table.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "memory.h"
#include "pair.h"
#include "root.h"
#include "symbol.h"
#include "vector.h"

// Returns true if the string object is deep equal to c_string.
b64 IsSymbolEqual(Object symbol, const char *name);

// Returns the index into the symbol_table where name belongs.
u64 GetSymbolListIndex(Object symbol_table, const char *name);
// Returns the symbol or nil.
Object FindSymbolInSymbolList(Object symbol_table, u64 index, const char *name);
// Removes symbol with the given name from the list of symbols.
// Returns the updated symbols list.
Object RemoveSymbolDestructively(Object symbols, const char *name);
// Returns a list of list_b appended to the end of list_a
Object Append(Object list_a, Object list_b);
// Returns list but reversed.
Object Reverse(Object list);

// Return the global symbol table
Object GetSymbolTable();

void InitializeSymbolTable(u64 size) {
  SetRegister(REGISTER_SYMBOL_TABLE, MakeSymbolTable(size));
}

// A Symbol table is a Hashed set, represented as a vector of Symbol Lists
Object MakeSymbolTable(u64 size) {
  return AllocateVector(size);
}


Object FindSymbol(const char *name) {
  Object symbol_table = GetSymbolTable();
  u64 index = GetSymbolListIndex(symbol_table, name);
  return FindSymbolInSymbolList(symbol_table, index, name);
}

Object InternSymbol(const char *name) {
  Object symbol_table = GetSymbolTable();
  u64 index = GetSymbolListIndex(symbol_table, name);
  Object found = FindSymbolInSymbolList(symbol_table, index, name);
  if (found != nil)
    return found;

  // Symbol not found
  // Create a new symbol and add it to the symbol list
  Object new_symbol = AllocateSymbol(name);
  Object old_symbols = VectorRef(GetSymbolTable(), index);
  Object new_symbols = MakePair(new_symbol, old_symbols);

  VectorSet(GetSymbolTable(), index, new_symbols);

  return FindSymbol(name);
}

void UninternSymbol(const char *name) {
  Object symbol_table = GetSymbolTable();
  u64 index = GetSymbolListIndex(symbol_table, name);
  Object symbols = VectorRef(symbol_table, index);

  Object new_symbols = RemoveSymbolDestructively(symbols, name);
  VectorSet(symbol_table, index, new_symbols);
}


// Helpers

u64 GetSymbolListIndex(Object symbol_table, const char *name) {
  u32 hash = HashString(name);
  s64 length = VectorLength(symbol_table);
  return hash % length;
}

Object RemoveSymbolDestructively(Object symbols, const char *name) {
  if (symbols == nil) {
    // CASE: ()
    return nil;
  } else if (IsSymbolEqual(First(symbols), name)) {
    // CASE: (symbol . rest)
    return Rest(symbols);
  } else {
    // CASE: element may be somewhere in the rest of the list.
    // symbols := (not-symbol . rest)
    Object result = symbols;

    // prev := (not-symbol . rest)
    // symbols := rest
    Object prev = symbols;
    symbols = Rest(symbols);

    for (; symbols != nil; prev = symbols, symbols = Rest(symbols)) {
      // prev := (x . y . rest)
      // symbols := (y . rest)

      Object symbol = First(symbols);
      if (IsSymbolEqual(symbol, name)) {
        // prev := (x . symbol . rest)
        // symbols := (symbol . rest)
        SetCdr(prev, Rest(symbols));
        // prev := (x . rest)
        return result;
      }
    }

    return result;
  }
}

Object FindSymbolInSymbolList(Object symbol_table, u64 index, const char *name) {
  for (Object symbols = VectorRef(symbol_table, index);
      symbols != nil;
      symbols = Rest(symbols)) {
    Object symbol = First(symbols);
    if (IsSymbolEqual(symbol, name))
      return symbol;
  }
  return nil;
}

b64 IsSymbolEqual(Object symbol, const char *name) {
  assert(IsSymbol(symbol));

  u64 symbol_reference = UnboxReference(symbol);
  char *symbol_string = (char*)&memory.the_objects[symbol_reference + 1];

  return strcmp(symbol_string, name) == 0;
}

// DJB2 algorithm by Dan Bernstein
u32 HashString(const u8 *str) {
  u32 hash = 5381;
  for (u8 c = *str; c; c = *str++)
    hash = hash*33 + c;
  return hash;
}

Object GetSymbolTable() {
  return GetRegister(REGISTER_SYMBOL_TABLE);
}

void TestSymbolTable() {
  assert(HashString("symbol") == 2905944654);

  InitializeMemory(128);
  InitializeSymbolTable(13);

  Object symbol = FindSymbol("symbol");
  assert(symbol == nil);

  symbol = InternSymbol("symbol");
  PrintlnObject(symbol);

  Object otherSymbol = FindSymbol("symbol");
  assert(symbol == otherSymbol);
  otherSymbol = InternSymbol("symbol");
  assert(symbol == otherSymbol);

  UninternSymbol("symbol");
  assert(FindSymbol("symbol") == nil);

  DestroyMemory();
  InitializeMemory(128);
  InitializeSymbolTable(1);
  InternSymbol("symbol");
  InternSymbol("dimple");
  InternSymbol("pimple");
  InternSymbol("limp-pole");
  assert(FindSymbol("symbol") != nil);
  UninternSymbol("dimple");
  assert(FindSymbol("dimple") == nil);
  assert(FindSymbol("symbol") != nil);
  assert(FindSymbol("pimple") != nil);
  DestroyMemory();
}
