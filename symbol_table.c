#include "symbol_table.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "memory.h"
#include "pair.h"
#include "root.h"
#include "string.h"
#include "symbol.h"
#include "vector.h"

// Returns true if the string object is deep equal the string name
b64 IsSymbolEqual(Object symbol, Object name);

// Returns the index into the symbol_table where name belongs.
u64 GetSymbolListIndex(Object symbol_table, Object name);
// Returns the symbol or nil.
Object FindSymbolInSymbolList(Object symbol_table, u64 index, Object name);
// Removes symbol with the given name from the list of symbols.
// Returns the updated symbols list.
Object RemoveSymbolDestructively(Object symbols, Object name);
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


Object FindSymbol(Object name) {
  Object symbol_table = GetSymbolTable();
  u64 index = GetSymbolListIndex(symbol_table, name);
  return FindSymbolInSymbolList(symbol_table, index, name);
}

Object InternSymbol(Object name) {
  Object symbol_table = GetSymbolTable();
  u64 index = GetSymbolListIndex(symbol_table, name);
  Object found = FindSymbolInSymbolList(symbol_table, index, name);
  if (found != nil)
    return found;

  // Symbol not found
  // Create a new symbol and add it to the symbol list
  Object new_symbol = BoxSymbol(name);
  Object old_symbols = VectorRef(GetSymbolTable(), index);
  Object new_symbols = MakePair(new_symbol, old_symbols);
  // REFERENCES INVALIDATED

  VectorSet(GetSymbolTable(), index, new_symbols);

  return FindSymbol(BoxString(Car(new_symbols)));
}

void UninternSymbol(Object name) {
  Object symbol_table = GetSymbolTable();
  u64 index = GetSymbolListIndex(symbol_table, name);
  Object symbols = VectorRef(symbol_table, index);

  Object new_symbols = RemoveSymbolDestructively(symbols, name);
  VectorSet(symbol_table, index, new_symbols);
}


// Helpers

u64 GetSymbolListIndex(Object symbol_table, Object name) {
  u32 hash = HashString(StringCharacterBuffer(name));
  s64 length = VectorLength(symbol_table);
  return hash % length;
}

Object RemoveSymbolDestructively(Object symbols, Object name) {
  if (symbols == nil) {
    // CASE: ()
    return nil;
  } else if (IsSymbolEqual(First(symbols), name)) {
    // CASE: (symbol . rest)
    return Rest(symbols);
  } else {
    // CASE: (not-symbol . rest)
    // Symbol may be in rest.
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

Object FindSymbolInSymbolList(Object symbol_table, u64 index, Object name) {
  for (Object symbols = VectorRef(symbol_table, index);
      symbols != nil;
      symbols = Rest(symbols)) {
    Object symbol = First(symbols);
    if (IsSymbolEqual(symbol, name))
      return symbol;
  }
  return nil;
}

b64 IsSymbolEqual(Object symbol, Object name) {
  assert(IsSymbol(symbol));
  assert(IsString(name));
  return strcmp(StringCharacterBuffer(symbol), StringCharacterBuffer(name)) == 0;
}

// DJB2 algorithm by Dan Bernstein
u32 HashString(const u8 *str) {
  u32 hash = 5381;
  for (u8 c = *str; c; c = *str++)
    hash = hash*33 + c;
  return hash;
}

Object GetSymbolTable() {
  Object table = GetRegister(REGISTER_SYMBOL_TABLE);
  assert(table != nil);
  return table;
}

void TestSymbolTable() {
  assert(HashString("symbol") == 2905944654);

  InitializeMemory(128);
  InitializeSymbolTable(13);

  Object symbol_name = AllocateString("symbol");

  Object symbol = FindSymbol(symbol_name);
  assert(symbol == nil);

  symbol = InternSymbol(symbol_name);
  PrintlnObject(symbol);

  Object otherSymbol = FindSymbol(symbol_name);
  assert(symbol == otherSymbol);
  otherSymbol = InternSymbol(symbol_name);
  assert(symbol == otherSymbol);

  UninternSymbol(symbol_name);
  assert(FindSymbol(symbol_name) == nil);

  DestroyMemory();
  InitializeMemory(128);
  InitializeSymbolTable(1);
  InternSymbol(symbol_name);
  InternSymbol(AllocateString("dimple"));
  InternSymbol(AllocateString("pimple"));
  InternSymbol(AllocateString("limp-pole"));
  assert(FindSymbol(symbol_name) != nil);
  UninternSymbol(AllocateString("dimple"));
  assert(FindSymbol(AllocateString("dimple")) == nil);
  assert(FindSymbol(symbol_name) != nil);
  assert(FindSymbol(AllocateString("pimple")) != nil);
  DestroyMemory();
}
