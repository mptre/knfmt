#ifndef OPTIONS_H
#define OPTIONS_H

static const char traces[] = {
	'a',	/* all */
	'c',	/* clang */
	'C',	/* cpp */
	'd',	/* doc */
	'D',	/* diff */
	'f',	/* func:line */
	'l',	/* lexer */
	'p',	/* parser */
	's',	/* style */
	'S',	/* simple */
	't',	/* token */
};

struct options {
	unsigned int	op_trace[sizeof(traces)];

	unsigned int	diff:1,
			diffparse:1,
			inplace:1,
			simple:1,
			test:1;
};

void		options_init(struct options *);
int		options_trace_parse(struct options *, const char *);
unsigned int	options_trace_level(const struct options *, char);

#endif
