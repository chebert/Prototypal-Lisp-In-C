#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

enum LogCategory {
  LOG_TEST     = 1 << 0,
  LOG_MEMORY   = 1 << 1,
  LOG_READ     = 1 << 2,
  LOG_EVALUATE = 1 << 3,
};

#define ALL_LOGS (LOG_TEST | LOG_MEMORY | LOG_READ | LOG_EVALUATE)
#define ENABLED_LOGS (LOG_TEST)

void Log(const char *function, const char *file, int line, const char *fmt, ...);
void LogError(const char *function, const char *file, int line, const char *fmt, ...);

#define LOG_ERROR(args...) LogError(__FUNCTION__, __FILE__, __LINE__, args)

#define LOG(category, args...) \
  do { if (category & ENABLED_LOGS) Log(__FUNCTION__, __FILE__, __LINE__, args); } while (0)
#define LOG_OP(category, op) \
  do { if (category & ENABLED_LOGS) { LOG(category, ""); op; } } while (0)

#endif
