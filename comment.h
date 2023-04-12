struct options;
struct style;
struct token;

struct buffer	*comment_trim(const struct token *, const struct style *,
    const struct options *);
