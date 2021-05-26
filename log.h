#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

void Log(const char *function, const char *file, int line, const char *fmt, ...);
void LogError(const char *function, const char *file, int line, const char *fmt, ...);

#define LOG(args...)            Log(__FUNCTION__, __FILE__, __LINE__, args)
#define LOG_ERROR(args...) LogError(__FUNCTION__, __FILE__, __LINE__, args)

#endif
