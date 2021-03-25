#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

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
