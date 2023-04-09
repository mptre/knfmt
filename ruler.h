struct doc;
struct token;

struct ruler {
	struct ruler_column	*rl_columns;	/* VECTOR(struct ruler_column) */
	struct ruler_indent	*rl_indent;	/* VECTOR(struct ruler_indent) */
	unsigned int		 rl_align;
	unsigned int		 rl_flags;
/*
 * Minimum alignment, use smallest possible alignment to align all columns.
 */
#define RULER_ALIGN_MIN		0x00000001u
/*
 * Maximum alignment, the aligment for all columns will be less or equal to the
 * alignment given to ruler_init().
 */
#define RULER_ALIGN_MAX		0x00000002u
/*
 * Fixed alignment, unconditionally align all columns to the alignment given to
 * ruler_init().
 */
#define RULER_ALIGN_FIXED	0x00000004u
/*
 * Minimum alignment, use smallest possible alignment ceiled to a multiple of 8
 * to align all columns.
 */
#define RULER_ALIGN_TABS	0x00000008u
/*
 * Sense and honor existing alignment.
 */
#define RULER_ALIGN_SENSE	0x00000020u
};

void	ruler_init(struct ruler *, unsigned int, unsigned int);
void	ruler_free(struct ruler *);
void	ruler_exec(struct ruler *);

#define ruler_insert(a, b, c, d, e, f) \
	ruler_insert0((a), (b), (c), (d), (e), (f), __func__, __LINE__)
void	ruler_insert0(struct ruler *, struct token *, struct doc *,
    unsigned int, unsigned int, unsigned int, const char *, int);

#define ruler_indent(a, b, c) \
	ruler_indent0((a), (b), (c), 1, __func__, __LINE__)
#define ruler_dedent(a, b, c) \
	ruler_indent0((a), (b), (c), -1, __func__, __LINE__)
struct doc	*ruler_indent0(struct ruler *, struct doc *,
    struct ruler_indent **, int, const char *, int);
void		 ruler_remove(struct ruler *, const struct ruler_indent *);
