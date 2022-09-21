static const char traces[] = {
	'a',	/* all */
	'd',	/* doc */
	'D',	/* diff */
	'l',	/* lexer */
	's',	/* style */
	'S',	/* simple */
};

struct options {
	unsigned int	op_trace[sizeof(traces)];

	unsigned int	op_flags;
#define OPTIONS_DIFF			0x00000001u
#define OPTIONS_DIFFPARSE		0x00000002u
#define OPTIONS_INPLACE			0x00000004u
#define OPTIONS_SIMPLE			0x00000008u
#define OPTIONS_TEST			0x80000000u
};

void	options_init(struct options *);
int	options_trace_parse(struct options *, const char *);

unsigned int	trace(const struct options *, unsigned char);
