struct parser;
struct parser_simple;

enum simple_state {
	SIMPLE_STATE_NOP	= -1,
	SIMPLE_STATE_DISABLE	= 0,
	SIMPLE_STATE_ENABLE	= 1,
	SIMPLE_STATE_IGNORE	= 2,
};

int	parser_simple_active(const struct parser *);
void	parser_simple_disable(struct parser *, struct parser_simple *);
void	parser_simple_enable(struct parser *, const struct parser_simple *);
