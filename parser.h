struct arena;
struct arena_scope;
struct buffer;
struct diffchunk;
struct lexer;
struct options;
struct simple;
struct style;

struct parser_doc_scope_cookie {
	struct arena_scope	**restore_scope;
	struct arena_scope	 *old_scope;
};

struct parser	*parser_alloc(struct lexer *, const struct style *,
    struct simple *, struct arena_scope *, struct arena *, struct arena *,
    const struct options *);
int		 parser_exec(struct parser *, const struct diffchunk *,
    struct buffer *);

#define parser_arena_scope(cookie, old_scope, new_scope)		\
	__attribute__((cleanup(parser_arena_scope_leave)))		\
		struct parser_doc_scope_cookie cookie;			\
	parser_arena_scope_enter(&(cookie), (old_scope), (new_scope))
void	parser_arena_scope_enter(struct parser_doc_scope_cookie *,
    struct arena_scope **, struct arena_scope *);

void	parser_arena_scope_leave(struct parser_doc_scope_cookie *);
