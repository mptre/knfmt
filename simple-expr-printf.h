struct arena_scope;
struct lexer;
struct token;

void	simple_expr_printf(struct lexer *, struct token *,
    struct arena_scope *);
