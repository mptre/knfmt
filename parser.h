struct arena_scope;
struct buffer;
struct diffchunk;

struct parser_arg {
	struct lexer		*lexer;
	const struct options	*options;
	const struct style	*style;
	struct simple		*simple;
	struct clang		*clang;

	struct {
		struct arena		*scratch;
		struct arena		*doc;
		struct arena		*buffer;
		struct arena		*ruler;
	} arena;
};

struct parser	*parser_alloc(const struct parser_arg *, struct arena_scope *);
int		 parser_exec(struct parser *, const struct diffchunk *,
    struct buffer *);
