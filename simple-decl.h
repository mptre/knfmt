struct arena_scope;
struct lexer;
struct options;
struct token;

struct simple_decl	*simple_decl_enter(struct lexer *,
    struct arena_scope *, const struct options *);
void			 simple_decl_leave(struct simple_decl *);
void			 simple_decl_free(struct simple_decl *);

void	simple_decl_type(struct simple_decl *, struct token *, struct token *);
void	simple_decl_semi(struct simple_decl *, struct token *);
void	simple_decl_comma(struct simple_decl *, struct token *);
