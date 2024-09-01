struct buffer;
struct diffchunk;

struct parser_arg {
	struct lexer		*lexer;
	const struct options	*options;
	const struct style	*style;
	struct simple		*simple;
	struct clang		*clang;

	struct {
		struct arena_scope	*eternal_scope;
		struct arena		*scratch;
		struct arena		*doc;
		struct arena		*buffer;
	} arena;
};

struct parser	*parser_alloc(const struct parser_arg *);
int		 parser_exec(struct parser *, const struct diffchunk *,
    struct buffer *);
