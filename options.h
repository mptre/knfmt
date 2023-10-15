static const char traces[] = {
	'a',	/* all */
	'c',	/* clang */
	'C',	/* cpp */
	'd',	/* doc */
	'D',	/* diff */
	'f',	/* func:line */
	'l',	/* lexer */
	's',	/* style */
	'S',	/* simple */
	't',	/* token */
};

struct options {
	unsigned int	op_trace[sizeof(traces)];

	struct {
		unsigned int	diff:1;
		unsigned int	diffparse:1;
		unsigned int	inplace:1;
		unsigned int	simple:1;
		unsigned int	test:1;
	} op_flags;
};

void	options_init(struct options *);
int	options_trace_parse(struct options *, const char *);

unsigned int	trace(const struct options *, char);
