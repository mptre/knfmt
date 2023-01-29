struct options;
struct style;
struct token;

char	*comment_trim(const struct token *, const struct style *,
    const struct options *);
