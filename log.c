#include "log.h"

#include <stdio.h>

void LogFormat(const char* tag, const char *function, const char *file, int line, const char* fmt, va_list args) {
  printf("[%s] %s:%s:%d ", tag, function, file, line);
  vprintf(fmt, args);
  printf("\n");
}

void LogError(const char *function, const char *file, int line, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  LogFormat("error", function, file, line, fmt, args);
  va_end(args);
}

void Log(const char *function, const char *file, int line, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  LogFormat("debug", function, file, line, fmt, args);
  va_end(args);
}
