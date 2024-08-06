#ifndef OPTIONS_H
#define OPTIONS_H

#include "trace-types.h"

struct options {
	unsigned int	trace[TRACE_MAX];
	unsigned int	diff:1,
			diffparse:1,
			inplace:1,
			simple:1;
};

void		options_init(struct options *);
int		options_trace_parse(struct options *, const char *);

static inline unsigned int
options_trace_level(const struct options *op, enum trace_type type)
{
	return op->trace[type];
}

#endif
