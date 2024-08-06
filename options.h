#ifndef OPTIONS_H
#define OPTIONS_H

#include "trace.h"

struct options {
	unsigned int	op_trace[TRACE_MAX];

	unsigned int	diff:1,
			diffparse:1,
			inplace:1,
			simple:1;
};

void		options_init(struct options *);
int		options_trace_parse(struct options *, const char *);
unsigned int	options_trace_level(const struct options *, char);

#endif
