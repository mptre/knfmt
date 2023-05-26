struct options;
struct lexer;
struct style;
struct token;

struct simple_stmt	*simple_stmt_enter(struct lexer *,
    const struct style *, const struct options *);
void			 simple_stmt_leave(struct simple_stmt *);
void			 simple_stmt_free(struct simple_stmt *);
struct doc		*simple_stmt_braces_enter(struct simple_stmt *,
    struct token *, struct token *, unsigned int);
struct doc		*simple_stmt_no_braces_enter(struct simple_stmt *,
    struct token *, unsigned int, void **);
void			 simple_stmt_no_braces_leave(struct simple_stmt *,
    struct token *, void *);
