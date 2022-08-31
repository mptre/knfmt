struct doc;
struct token;

struct ruler {
	struct ruler_column	*rl_columns;	/* VECTOR(struct ruler_column) */
	struct ruler_indent	*rl_indent;	/* VECTOR(struct ruler_indent) */
	int			 rl_len;
};

void	ruler_init(struct ruler *, int);
void	ruler_free(struct ruler *);
void	ruler_exec(struct ruler *);

#define ruler_insert(a, b, c, d, e, f) \
	__ruler_insert((a), (b), (c), (d), (e), (f), __func__, __LINE__)
void	__ruler_insert(struct ruler *, const struct token *, struct doc *,
    unsigned int, unsigned int, unsigned int, const char *, int);

#define ruler_indent(a, b, c) \
	__ruler_indent((a), (b), (c), 1, __func__, __LINE__)
#define ruler_dedent(a, b, c) \
	__ruler_indent((a), (b), (c), -1, __func__, __LINE__)
struct doc	*__ruler_indent(struct ruler *, struct doc *,
    struct ruler_indent **, int, const char *, int);
void		 ruler_remove(struct ruler *, struct ruler_indent *);
