struct arena;
struct arena_scope;
struct buffer;
struct diffchunk;
struct lexer;
struct options;
struct simple;
struct style;

struct parser_doc_scope_cookie {
	struct parser		*parser;
	struct arena_scope	*doc_scope;
};

struct parser	*parser_alloc(struct lexer *, const struct style *,
    struct simple *, struct arena_scope *, struct arena *, struct arena *,
    const struct options *);
void		 parser_free(struct parser *);
int		 parser_exec(struct parser *, const struct diffchunk *,
    struct buffer *);

#define parser_doc_scope(pr, cookie, scope)				\
	__attribute__((cleanup(parser_doc_scope_leave)))		\
		struct parser_doc_scope_cookie cookie;			\
	parser_doc_scope_enter((pr), &(cookie), (scope))
void	parser_doc_scope_enter(struct parser *,
    struct parser_doc_scope_cookie *, struct arena_scope *);

void	parser_doc_scope_leave(struct parser_doc_scope_cookie *);
