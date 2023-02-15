struct parser;

struct parser_stmt_block_arg {
	struct doc	*head;
	struct doc	*tail;
	struct doc	*rbrace;
	unsigned int	 flags;
#define PARSER_STMT_BLOCK_SWITCH		0x00000001u
#define PARSER_STMT_BLOCK_TRIM			0x00000002u
#define PARSER_STMT_BLOCK_EXPR_GNU		0x00000004u
};

int	parser_stmt_block(struct parser *, struct parser_stmt_block_arg *);
