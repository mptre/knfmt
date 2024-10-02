#include <stddef.h>	/* size_t */

#include "lexer-callbacks.h"

#define LEXER_EOF	0x7fffffff

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

	struct {
		struct arena_scope	*eternal_scope;
		struct arena		*scratch;
	} arena;

	struct lexer_callbacks	 callbacks;
};

struct lexer_buffer {
	const char	*ptr;
	size_t		 len;
};

struct lexer_state {
	struct token	*st_tk;
	size_t		 st_off;
	unsigned int	 st_lno;
	struct {
		unsigned int	error:1,
				eof:1;
	} st_flags;
};

struct lexer	*lexer_tokenize(const struct lexer_arg *);

struct lexer_state	lexer_get_state(const struct lexer *);
void			lexer_set_state(struct lexer *,
    const struct lexer_state *);

const char	*lexer_get_path(const struct lexer *);
int		 lexer_get_peek(const struct lexer *);

int		 lexer_getc(struct lexer *, unsigned char *);
void		 lexer_ungetc(struct lexer *);
void		 lexer_eat_lines_and_spaces(struct lexer *,
    struct lexer_state *);
struct token	*lexer_emit(struct lexer *, const struct lexer_state *, int);
struct token	*lexer_emit_template(struct lexer *, const struct lexer_state *,
    const struct token *);
struct token	*lexer_emit_synthetic(struct lexer *, const struct token *);
const char	*lexer_serialize(struct lexer *, const struct token *);
int		 lexer_eat_lines(struct lexer *, int, struct token **);
int		 lexer_eat_spaces(struct lexer *, struct token **);

void	lexer_error(struct lexer *, const struct token *, const char *, int,
    const char *, ...) __attribute__((__format__(printf, 5, 6)));
void	lexer_error_flush(struct lexer *);
void	lexer_error_reset(struct lexer *);

void	lexer_buffer_peek(const struct lexer *, struct lexer_buffer *);
int	lexer_buffer_slice(const struct lexer *,
    const struct lexer_state *, struct lexer_buffer *);
void	lexer_buffer_seek(struct lexer *, size_t);

int		lexer_eof(const struct lexer *);
unsigned int	lexer_get_error(const struct lexer *);
int		lexer_get_lines(const struct lexer *, unsigned int,
    unsigned int, const char **, size_t *);
unsigned int	lexer_column(const struct lexer *, const struct lexer_state *);

void	lexer_seek(struct lexer *, struct token *);
int	lexer_seek_after(struct lexer *, struct token *);

int	lexer_pop(struct lexer *, struct token **);
int	lexer_back(const struct lexer *, struct token **);
int	lexer_back_if(const struct lexer *, int, struct token **);

struct token	*lexer_copy_after(struct lexer *, struct token *,
    const struct token *);
struct token	*lexer_insert_after(struct lexer *, struct token *,
    const struct token *);
struct token	*lexer_move_after(struct lexer *, struct token *,
    struct token *);
struct token	*lexer_move_before(struct lexer *, struct token *,
    struct token *);
void		 lexer_remove(struct lexer *, struct token *);
void		 lexer_move_prefixes(struct lexer *, struct token *,
    struct token *);

#define lexer_expect(a, b, c) \
	lexer_expect_impl((a), (b), (c), __func__, __LINE__)
int	lexer_expect_impl(struct lexer *, int, struct token **,
    const char *, int);

void	lexer_peek_enter(struct lexer *, struct lexer_state *);
void	lexer_peek_leave(struct lexer *, const struct lexer_state *);

int	lexer_peek(struct lexer *, struct token **);

int	lexer_peek_if(struct lexer *, int, struct token **);
int	lexer_if(struct lexer *, int, struct token **);

int	lexer_peek_if_flags(struct lexer *, unsigned int, struct token **);
int	lexer_if_flags(struct lexer *, unsigned int, struct token **);

int	lexer_peek_if_pair(struct lexer *, int, int, struct token **,
    struct token **);
int	lexer_if_pair(struct lexer *, int, int, struct token **,
    struct token **);

int	lexer_peek_if_prefix_flags(struct lexer *, unsigned int,
    struct token **);

int	lexer_peek_until(struct lexer *, int, struct token **);
int	lexer_peek_until_comma(struct lexer *, const struct token *,
    struct token **);
int	lexer_until(struct lexer *, int, struct token **);

int	lexer_peek_first(struct lexer *, struct token **);
int	lexer_peek_last(struct lexer *, struct token **);

void	lexer_dump(struct lexer *);
