struct arena_scope;
struct lexer;
struct token;

struct simple_decl_forward	*simple_decl_forward_enter(struct lexer *,
    struct arena_scope *);
void				 simple_decl_forward_leave(
    struct simple_decl_forward *);
void				 simple_decl_forward_free(
    struct simple_decl_forward *);
void				 simple_decl_forward(
    struct simple_decl_forward *, struct token *, struct token *);
