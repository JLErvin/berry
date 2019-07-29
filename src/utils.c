#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>


#include "types.h"
#include "utils.h"

int
asprintf(char **buf, const char *fmt, ...)
{
	int size = 0;
	va_list args;
	va_start(args, fmt);
	size = vasprintf(buf, fmt, args);
	va_end(args);
	return size;
}

int
vasprintf(char **buf, const char *fmt, va_list args)
{
	va_list tmp;
	va_copy(tmp, args);
	int size = vsnprintf(NULL, 0, fmt, tmp);
	va_end(tmp);

	if (size < 0) {
		return -1;
	}

	*buf = malloc(size + 1);

	if (*buf == NULL) {
		return -1;
	}

	size = vsprintf(*buf, fmt, args);
	return size;
}

int
euclidean_distance(struct client *a, struct client *b)
{
    int x_diff, y_diff;
    x_diff = a->geom.x - b->geom.x;
    y_diff = a->geom.y - b->geom.y;
    return pow(x_diff, 2) + pow(y_diff, 2);
}

int
round_k(int n)
{
    return ceil(n / 10) * 10;
}
