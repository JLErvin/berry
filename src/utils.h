#ifndef _BERRY_UTILS_H_
#define _BERRY_UTILS_H_

#include <X11/Xlib.h>

#include "types.h"

#define MAX(a, b) ((a > b) ? (a) : (b))

int euclidean_distance(struct client *a, struct client *b);

#endif
