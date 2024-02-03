struct arena;
struct arena_scope;
struct lexer;
struct style;

void	cpp_include_guard(const struct style *, struct lexer *,
    struct arena_scope *, struct arena *);
