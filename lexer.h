#include <stddef.h>	/* size_t */

#define LEXER_EOF	0x7fffffff

struct arena_scope;
struct lexer;

struct lexer_arg {
	const char		*path;
	const struct buffer	*bf;
	struct diffchunk	*diff;
	const struct options	*op;

	/*
	 * Report errors immediately, removing the need to call
	 * lexer_error_flush() explicitly.
	 */
	int			 error_flush;

	struct lexer_callbacks {
		/*
		 * Read callback with the following semantics:
		 *
		 *     1. In case of encountering an error, NULL must be
		 *        returned.
		 *     2. Signalling the reach of end of file is done by
		 *        returning a token with type LEXER_EOF.
		 *     3. If none of the above occurs, the next consumed token
		 *        is assumed to be returned.
		 */
		struct token	*(*read)(struct lexer *, void *);

		/*
		 * Allocate a new token from the given arena scope.
		 * Expected to be initialized using the given token.
		 */
		struct token	*(*alloc)(struct arena_scope *,
		    const struct token *);

		/*
		 * Serialize routine used to turn the given token into something
		 * human readable.
		 */
		const char	*(*serialize)(struct arena_scope *,
		    const struct token *);

		/* Opaque argument passed to callbacks. */
		void		*arg;
	} callbacks;
};

struct lexer_state {
	struct token	*st_tk;
	unsigned int	 st_lno;
	unsigned int	 st_err;
	size_t		 st_off;
};

struct lexer	*lexer_alloc(const struct lexer_arg *);
void		 lexer_free(struct lexer *);

struct lexer_state	lexer_get_state(const struct lexer *);
void			lexer_set_state(struct lexer *,
    const struct lexer_state *);

const char	*lexer_get_path(const struct lexer *);

int		 lexer_getc(struct lexer *, unsigned char *);
void		 lexer_ungetc(struct lexer *);
size_t		 lexer_match(struct lexer *, const char *);
void		 lexer_eat_lines_and_spaces(struct lexer *,
    struct lexer_state *);
struct token	*lexer_emit(struct lexer *, const struct lexer_state *,
    const struct token *);
const char	*lexer_serialize(struct lexer *, const struct token *);
int		 lexer_eat_lines(struct lexer *, int, struct token **);
int		 lexer_eat_spaces(struct lexer *, struct token **);

void	lexer_error(struct lexer *, const struct token *, const char *, int,
    const char *, ...) __attribute__((__format__(printf, 5, 6)));
void	lexer_error_flush(struct lexer *);
void	lexer_error_reset(struct lexer *);

int		 lexer_buffer_streq(const struct lexer *,
    const struct lexer_state *, const char *);
const char	*lexer_buffer_slice(const struct lexer *,
    const struct lexer_state *, size_t *);

int		lexer_eof(const struct lexer *);
unsigned int	lexer_get_error(const struct lexer *);
int		lexer_get_lines(const struct lexer *, unsigned int,
    unsigned int, const char **, size_t *);

void	lexer_stamp(struct lexer *);
int	lexer_recover(struct lexer *);
int	lexer_branch(struct lexer *);
int	lexer_seek(struct lexer *, struct token *);
int	lexer_seek_after(struct lexer *, struct token *);

int	lexer_is_branch(const struct lexer *);

int	lexer_pop(struct lexer *, struct token **);
int	lexer_back(const struct lexer *, struct token **);
int	lexer_back_if(const struct lexer *, int, struct token **);

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
	lexer_expect0((a), (b), (c), __func__, __LINE__)
int	lexer_expect0(struct lexer *, int, struct token **,
    const char *, int);

void	lexer_peek_enter(struct lexer *, struct lexer_state *);
void	lexer_peek_leave(struct lexer *, const struct lexer_state *);

int	lexer_peek(struct lexer *, struct token **);

int	lexer_peek_if(struct lexer *, int, struct token **);
int	lexer_if(struct lexer *, int, struct token **);

int	lexer_peek_if_flags(struct lexer *, unsigned int, struct token **);
int	lexer_if_flags(struct lexer *, unsigned int, struct token **);

int	lexer_peek_if_pair(struct lexer *, int, int, struct token **);
int	lexer_if_pair(struct lexer *, int, int, struct token **);

int	lexer_peek_if_prefix_flags(struct lexer *, unsigned int,
    struct token **);

int	lexer_peek_until(struct lexer *, int, struct token **);
int	lexer_peek_until_comma(struct lexer *, const struct token *,
    struct token **);
int	lexer_until(struct lexer *, int, struct token **);

void	lexer_dump(struct lexer *);
