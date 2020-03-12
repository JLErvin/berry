#ifndef _BERRY_UTILS_H_
#define _BERRY_UTILS_H_

#include <X11/Xlib.h>
#include <stdarg.h>

#include "types.h"

#define MAX(a, b) ((a > b) ? (a) : (b))
#define MIN(a, b) ((a < b) ? (a) : (b))
#define UNUSED(x) (void)(x)
#define LOGN(msg)      do { if (debug) fprintf(stderr, WINDOW_MANAGER_NAME": " msg "\n"); } while (0) 
#define LOGP(msg, ...) do { if (debug) fprintf(stderr, WINDOW_MANAGER_NAME": " msg "\n", __VA_ARGS__); } while (0)

int asprintf(char **buf, const char *fmt, ...);
int vasprintf(char **buf, const char *fmt, va_list args);
int euclidean_distance(struct client *a, struct client *b);
int round_k(int n);

#endif
