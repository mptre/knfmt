struct arena;
struct lexer;
struct style;

void	cpp_include_guard(const struct style *, struct lexer *,
    struct arena *);
