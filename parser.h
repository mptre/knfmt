struct arena;
struct arena_scope;
struct buffer;
struct diffchunk;
struct lexer;
struct options;
struct simple;
struct style;

#define CONCAT(x, y) CONCAT2(x, y)
#define CONCAT2(x, y) x ## y

struct parser_arena_scope_cookie {
	struct arena_scope	**restore_scope;
	struct arena_scope	 *old_scope;
};

struct parser	*parser_alloc(struct lexer *, const struct style *,
    struct simple *, struct arena_scope *, struct arena *, struct arena *,
    const struct options *);
int		 parser_exec(struct parser *, const struct diffchunk *,
    struct buffer *);

#define parser_arena_scope(old_scope, new_scope)			\
	__attribute__((cleanup(parser_arena_scope_leave)))		\
	    struct parser_arena_scope_cookie CONCAT(cookie_, __LINE__);	\
	parser_arena_scope_enter(&(CONCAT(cookie_, __LINE__)),		\
	    (old_scope), (new_scope))
void	parser_arena_scope_enter(struct parser_arena_scope_cookie *,
    struct arena_scope **, struct arena_scope *);

void	parser_arena_scope_leave(struct parser_arena_scope_cookie *);
