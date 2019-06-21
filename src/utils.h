#ifndef _BERRY_UTILS_H_
#define _BERRY_UTILS_H_

#include <X11/Xlib.h>

#include "types.h"

#define MAX(a, b) ((a > b) ? (a) : (b))
#define MIN(a, b) ((a < b) ? (a) : (b))
#define UNUSED(x) (void)(x)

int euclidean_distance(struct client *a, struct client *b);
int round_k(int n);

#endif
