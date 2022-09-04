struct config;
struct lexer;
struct token;

struct simple_stmt	*simple_stmt_enter(struct lexer *,
    const struct config *);
void			 simple_stmt_leave(struct simple_stmt *);
void			 simple_stmt_free(struct simple_stmt *);
struct doc		*simple_stmt_block(struct simple_stmt *,
    struct token *, struct token *, int);
void			*simple_stmt_ifelse_enter(struct simple_stmt *,
    struct token *, int);
void			 simple_stmt_ifelse_leave(struct simple_stmt *,
    struct token *, void *);

