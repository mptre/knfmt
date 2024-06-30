struct doc;
struct parser;

int	parser_asm_peek(struct parser *);
int	parser_root_asm(struct parser *, struct doc *);
int	parser_decl_asm(struct parser *, struct doc *);
int	parser_stmt_asm(struct parser *, struct doc *);
