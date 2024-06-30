struct arena_scope;
struct buffer;
struct diffchunk;

#define CONCAT(x, y) CONCAT2(x, y)
#define CONCAT2(x, y) x ## y

struct parser_arg {
	const struct options	*options;
	const struct style	*style;
	struct simple		*simple;
	struct lexer		*lexer;
	struct clang		*clang;

	struct {
		struct arena_scope	*eternal_scope;
		struct arena		*scratch;
		struct arena		*doc;
		struct arena		*buffer;
	} arena;
};

struct parser_arena_scope_cookie {
	struct arena_scope	**restore_scope;
	struct arena_scope	 *old_scope;
};

struct parser	*parser_alloc(const struct parser_arg *);
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
