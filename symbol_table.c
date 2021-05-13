#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "symbol_table.h"

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
Object RemoveSymbol(struct Memory *memory, Object symbols, const char *name);
// Returns a list of list_b appended to the end of list_a
Object Append(struct Memory *memory, Object list_a, Object list_b);
// Returns list but reversed.
Object Reverse(struct Memory *memory, Object list);

// A Symbol table is a Hashed set, represented as a vector of Symbol Lists
Object MakeSymbolTable(struct Memory *memory, u64 size) {
  return AllocateVector(memory, size);
}

Object FindSymbol(struct Memory *memory, Object symbol_table, const char *name) {
  u64 index = GetSymbolListIndex(memory, symbol_table, name);
  return FindSymbolInSymbolList(memory, symbol_table, index, name);
}

Object InternSymbol(struct Memory *memory, Object symbol_table, const char *name) {
  u64 index = GetSymbolListIndex(memory, symbol_table, name);
  Object found = FindSymbolInSymbolList(memory, symbol_table, index, name);
  if (found != nil)
    return found;

  // Symbol not found
  // Create a new symbol and add it to the symbol list
  Object new_symbol = AllocateSymbol(memory, name);
  Object symbols = VectorRef(memory, UnboxReference(symbol_table), index);
  Object new_symbols = AllocatePair(memory, new_symbol, symbols);
  VectorSet(memory, UnboxReference(symbol_table), index, new_symbols);
  return new_symbol;
}

void UninternSymbol(struct Memory *memory, Object symbol_table, const char *name) {
  u64 index = GetSymbolListIndex(memory, symbol_table, name);
  Object symbols = VectorRef(memory, UnboxReference(symbol_table), index);

  Object new_symbols = RemoveSymbol(memory, symbols, name);
  PrintlnObject(memory, new_symbols);
  VectorSet(memory, UnboxReference(symbol_table), index, new_symbols);
}


// Helpers

u64 GetSymbolListIndex(struct Memory *memory, Object symbol_table, const char *name) {
  u32 hash = HashString(name);
  s64 length = VectorLength(memory, UnboxReference(symbol_table));
  return hash % length;
}

Object Reverse(struct Memory *memory, Object list) {
  Object result = nil;
  for (; list != nil; list = Rest(memory, list))
    result = AllocatePair(memory, First(memory, list), result);
  return result;
}

Object Append(struct Memory *memory, Object list_a, Object list_b) {
  Object result = list_b;
  for (Object reversed_a = Reverse(memory, list_a);
      reversed_a != nil;
      reversed_a = Rest(memory, reversed_a)) {
    result = AllocatePair(memory, First(memory, reversed_a), result);
  }
  return result;
}

Object RemoveSymbol(struct Memory *memory, Object symbols, const char *name) {
  Object new_symbols = nil;
  for (; symbols != nil; symbols = Rest(memory, symbols)) {
    Object symbol = First(memory, symbols);
    // If the string matches, we can quit checking here.
    if (IsSymbolEqual(memory, symbol, name))
      return Append(memory, new_symbols, Rest(memory, symbols));
    new_symbols = AllocatePair(memory, symbol, new_symbols);
  }
  return new_symbols;
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

  struct Memory memory = AllocateMemory(128);
  Object list = AllocatePair(&memory, BoxFixnum(1), AllocatePair(&memory, BoxFixnum(2),
        AllocatePair(&memory, BoxFixnum(3), nil)));
  PrintlnObject(&memory, list);
  PrintlnObject(&memory, Reverse(&memory, list));

  PrintlnObject(&memory, Append(&memory, list, list));

  assert(Reverse(&memory, nil) == nil);
  assert(Append(&memory, nil, nil) == nil);

  Object table = MakeSymbolTable(&memory, 13);
  assert(GetSymbolListIndex(&memory, table, "symbol") == 2);

  Object symbol = FindSymbol(&memory, table, "symbol");
  assert(symbol == nil);

  symbol = InternSymbol(&memory, table, "symbol");
  PrintlnObject(&memory, symbol);

  Object otherSymbol = FindSymbol(&memory, table, "symbol");
  assert(symbol == otherSymbol);
  otherSymbol = InternSymbol(&memory, table, "symbol");
  assert(symbol == otherSymbol);

  UninternSymbol(&memory, table, "symbol");
  assert(FindSymbol(&memory, table, "symbol") == nil);
}
