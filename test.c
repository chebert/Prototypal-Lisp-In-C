#include "eval.h"
#include "memory.h"
#include "tag.h"
#include "token.h"
#include "read.h"
#include "symbol_table.h"

int main(int argc, char **argv) {
  TestTag();
  TestToken();
  TestMemory();
  TestSymbolTable();
  TestRead();
  TestEval();
  return 0;
}
