#include <X11/Xlib.h>
#include <math.h>

#include "types.h"
#include "utils.h"

int
euclidean_distance(struct client *a, struct client *b)
{
    int16_t x_diff, y_diff;
    x_diff = a->geom.x - b->geom.x;
    y_diff = a->geom.y - b->geom.y;
    return pow(x_diff, 2) + pow(y_diff, 2);
}
