#ifndef _BERRY_UTILS_H_
#define _BERRY_UTILS_H_

#include "types.h"
#include <stdarg.h>

#define MAX(a, b) ((a > b) ? (a) : (b))
#define MIN(a, b) ((a < b) ? (a) : (b))
#define UNUSED(x) (void)(x)
#define LOGN(msg)      do { if (debug) fprintf(stderr, __WINDOW_MANAGER_NAME__": " msg "\n"); } while (0)
#define LOGP(msg, ...) do { if (debug) fprintf(stderr, __WINDOW_MANAGER_NAME__": " msg "\n", __VA_ARGS__); } while (0)

int asprintf(char **buf, const char *fmt, ...);
int vasprintf(char **buf, const char *fmt, va_list args);

#endif
