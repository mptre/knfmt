#include "alloc.h"

#include "config.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>

void *
emalloc(size_t size)
{
	void *p;

	p = malloc(size);
	if (p == NULL)
		err(1, NULL);
	return p;
}

void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	p = calloc(nmemb, size);
	if (p == NULL)
		err(1, NULL);
	return p;
}

char *
estrdup(const char *str)
{
	char *p;

	p = strdup(str);
	if (p == NULL)
		err(1, NULL);
	return p;
}
