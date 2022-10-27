struct parser_context {
	const char		*pc_path;
	const struct options	*pc_op;
	struct lexer		*pc_lx;
	struct error		*pc_er;
	unsigned int		 pc_error;
};
