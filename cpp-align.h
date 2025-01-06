struct arenas;
struct lexer;
struct options;
struct style;
struct token;

const char	*cpp_align(const struct lexer *, struct token *,
    const struct style *,
    struct arenas *, const struct options *);
