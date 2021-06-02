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

Object InternNewSymbol(u64 index, const u8 *name, enum ErrorCode *error);

// Returns true if the string object is deep equal the string name
b64 IsSymbolEqual(Object symbol, const u8 *name);

// Returns the index into the symbol_table where name belongs.
u64 GetSymbolListIndex(Object symbol_table, const u8 *name);
// Returns the symbol or nil.
Object FindSymbolInSymbolList(Object symbol_table, u64 index, const u8 *name);
// Removes symbol with the given name from the list of symbols.
// Returns the updated symbols list.
Object RemoveSymbolDestructively(Object symbols, const u8 *name);
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
  enum ErrorCode error = NO_ERROR;
  Object table = AllocateVector(size, &error);
  assert(!error);
  return table;
}


Object FindSymbol(const u8 *name) {
  Object symbol_table = GetSymbolTable();
  u64 index = GetSymbolListIndex(symbol_table, name);
  return FindSymbolInSymbolList(symbol_table, index, name);
}

Object InternSymbol(const u8 *name, enum ErrorCode *error) {
  Object symbol_table = GetSymbolTable();
  u64 index = GetSymbolListIndex(symbol_table, name);
  Object found = FindSymbolInSymbolList(symbol_table, index, name);
  if (found != nil)
    return found;

  return InternNewSymbol(index, name, error);
}

void UninternSymbol(const u8 *name) {
  Object symbol_table = GetSymbolTable();
  u64 index = GetSymbolListIndex(symbol_table, name);
  Object symbols = UnsafeVectorRef(symbol_table, index);

  Object new_symbols = RemoveSymbolDestructively(symbols, name);
  UnsafeVectorSet(symbol_table, index, new_symbols);
}


// Helpers

Object InternNewSymbol(u64 index, const u8 *name, enum ErrorCode *error) {
  // Create a new symbol and add it to the symbol list
  Object new_symbols = AllocatePair(error);
  if (*error) return nil;
  // REFERENCES INVALIDATED

  Object old_symbols = UnsafeVectorRef(GetSymbolTable(), index);
  UnsafeVectorSet(GetSymbolTable(), index, new_symbols);

  SetCdr(new_symbols, old_symbols);
  SetCar(new_symbols, AllocateSymbol(name, error));
  if (*error) return nil;
  // REFERENCES INVALIDATED

  // Return the new symbol.
  return Car(UnsafeVectorRef(GetSymbolTable(), index));
}

u64 GetSymbolListIndex(Object symbol_table, const u8 *name) {
  u32 hash = HashString(name);
  s64 length = UnsafeVectorLength(symbol_table);
  return hash % length;
}

Object RemoveSymbolDestructively(Object symbols, const u8 *name) {
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

Object FindSymbolInSymbolList(Object symbol_table, u64 index, const u8 *name) {
  for (Object symbols = UnsafeVectorRef(symbol_table, index);
      symbols != nil;
      symbols = Rest(symbols)) {
    Object symbol = First(symbols);
    if (IsSymbolEqual(symbol, name))
      return symbol;
  }
  return nil;
}

b64 IsSymbolEqual(Object symbol, const u8 *name) {
  assert(IsSymbol(symbol));
  return strcmp(StringCharacterBuffer(symbol), name) == 0;
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

  const u8 *symbol_name = "symbol";

  Object symbol = FindSymbol(symbol_name);
  assert(symbol == nil);

  enum ErrorCode error = NO_ERROR;
  symbol = InternSymbol(symbol_name, &error);
  PrintlnObject(symbol);

  Object otherSymbol = FindSymbol(symbol_name);
  assert(symbol == otherSymbol);
  otherSymbol = InternSymbol(symbol_name, &error);
  assert(symbol == otherSymbol);

  UninternSymbol(symbol_name);
  assert(FindSymbol(symbol_name) == nil);

  DestroyMemory();
  InitializeMemory(128);
  InitializeSymbolTable(1);
  InternSymbol(symbol_name, &error);
  InternSymbol("dimple", &error);
  InternSymbol("pimple", &error);
  InternSymbol("limp-pole", &error);
  assert(FindSymbol(symbol_name) != nil);
  UninternSymbol("dimple");
  assert(FindSymbol("dimple") == nil);
  assert(FindSymbol(symbol_name) != nil);
  assert(FindSymbol("pimple") != nil);
  DestroyMemory();
}
