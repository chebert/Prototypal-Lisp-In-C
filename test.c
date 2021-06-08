#include "evaluate.h"
#include "memory.h"
#include "tag.h"
#include "read.h"
#include "symbol_table.h"

int main(int argc, char **argv) {
  TestTag();
  TestMemory();
  TestSymbolTable();
  TestRead();
  TestEvaluate();
  return 0;
}
