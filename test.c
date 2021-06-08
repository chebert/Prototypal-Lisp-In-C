#include "evaluate.h"
#include "memory.h"
#include "tag.h"
#include "token.h"
#include "read.h"
#include "read2.h"
#include "symbol_table.h"

int main(int argc, char **argv) {
  /*
  TestTag();
  TestToken();
  TestMemory();
  TestSymbolTable();
  TestRead();
  */
  TestRead2();
  TestEvaluate();
  return 0;
}
