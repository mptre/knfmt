#include <stddef.h>	/* size_t */

#define LEXER_EOF	0x7fffffff

struct buffer;
struct diffchunk;
struct error;
struct lexer;
struct options;

struct lexer_arg {
	const char		*path;
	const struct buffer	*bf;
	struct error		*er;
	struct diffchunk	*diff;
	const struct options	*op;

	struct {
		/*
		 * Read callback with the following semantics:
		 *
		 *     1. In case of encountering an error, NULL must be
		 *        returned.
		 *     2. Signalling the reach of end of file is done by
		 *        returning a token with type LEXER_EOF.
		 *     3. If none of the above occurs, the next consumed token
		 *        is assumed to be returned.
		 *
		 * Passing NULL will default to a callback capable of detecting
		 * C tokens.
		 */
		struct token	*(*read)(struct lexer *, void *);

		/*
		 * Serialize callback used to turn the given token into something
		 * human readable. Passing NULL will default to a callback
		 * capable of serializing C tokens.
		 */
		char		*(*serialize)(const struct token *);

		/* Opaque argument passed to callbacks. */
		void		*arg;
	} callbacks;
};

struct lexer_state {
	struct token	*st_tk;
	unsigned int	 st_lno;
	unsigned int	 st_cno;
	unsigned int	 st_err;
	size_t		 st_off;
};

void		 lexer_init(void);
void		 lexer_shutdown(void);
struct lexer	*lexer_alloc(const struct lexer_arg *);
void		 lexer_free(struct lexer *);

void		 lexer_get_state(const struct lexer *, struct lexer_state *);
int		 lexer_getc(struct lexer *, unsigned char *);
void		 lexer_ungetc(struct lexer *);
struct token	*lexer_emit(struct lexer *, const struct lexer_state *,
    const struct token *);
void		 lexer_error(struct lexer *, const char *, ...);

int	lexer_get_error(const struct lexer *);
int	lexer_get_lines(const struct lexer *, unsigned int,
    unsigned int, const char **, size_t *);

void	lexer_stamp(struct lexer *);
int	lexer_recover(struct lexer *);
int	lexer_branch(struct lexer *);

int	lexer_is_branch(const struct lexer *);

int	lexer_pop(struct lexer *, struct token **);
int	lexer_back(const struct lexer *, struct token **);

struct token	*lexer_copy_after(struct lexer *, struct token *,
    const struct token *);
struct token	*lexer_insert_before(struct lexer *, struct token *, int,
    const char *);
struct token	*lexer_insert_after(struct lexer *, struct token *, int,
    const char *);
struct token	*lexer_move_after(struct lexer *, struct token *,
    struct token *);
struct token	*lexer_move_before(struct lexer *, struct token *,
    struct token *);
void		 lexer_remove(struct lexer *, struct token *, int);

#define lexer_expect(a, b, c) \
	__lexer_expect((a), (b), (c), __func__, __LINE__)
int	__lexer_expect(struct lexer *, int, struct token **,
    const char *, int);

void	lexer_peek_enter(struct lexer *, struct lexer_state *);
void	lexer_peek_leave(struct lexer *, struct lexer_state *);

int	lexer_peek(struct lexer *, struct token **);

#define LEXER_TYPE_FLAG_CAST	0x00000001u
#define LEXER_TYPE_FLAG_ARG	0x00000002u

int	lexer_peek_if_type(struct lexer *, struct token **, unsigned int);
int	lexer_if_type(struct lexer *, struct token **, unsigned int);

int	lexer_peek_if(struct lexer *, int, struct token **);
int	lexer_if(struct lexer *, int, struct token **);

int	lexer_peek_if_flags(struct lexer *, unsigned int, struct token **);
int	lexer_if_flags(struct lexer *, unsigned int, struct token **);

int	lexer_peek_if_pair(struct lexer *, int, int, struct token **);
int	lexer_if_pair(struct lexer *, int, int, struct token **);

int	lexer_peek_if_prefix_flags(struct lexer *, unsigned int,
    struct token **);

int	lexer_peek_until(struct lexer *, int, struct token **);
int	lexer_peek_until_loose(struct lexer *, int, const struct token *,
    struct token **);
int	lexer_until(struct lexer *, int, struct token **);

const struct diffchunk	*lexer_get_diffchunk(const struct lexer *,
    unsigned int);

void	lexer_dump(const struct lexer *);
