#include <stddef.h>	/* size_t */

#include "token-type.h"

struct buffer;
struct diffchunk;
struct error;
struct options;

struct lexer_state {
	struct token	*st_tk;
	unsigned int	 st_lno;
	unsigned int	 st_cno;
	unsigned int	 st_err;
	size_t		 st_off;
};

void		 lexer_init(void);
void		 lexer_shutdown(void);
struct lexer	*lexer_alloc(const char *, const struct buffer *,
    struct error *, const struct diffchunk *, const struct options *);
void		 lexer_free(struct lexer *);

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
struct token	*lexer_insert_before(struct lexer *, struct token *,
    enum token_type, const char *);
struct token	*lexer_insert_after(struct lexer *, struct token *,
    enum token_type, const char *);
struct token	*lexer_move_after(struct lexer *, struct token *,
    struct token *);
struct token	*lexer_move_before(struct lexer *, struct token *,
    struct token *);
void		 lexer_remove(struct lexer *, struct token *, int);

#define lexer_expect(a, b, c) \
	__lexer_expect((a), (b), (c), __func__, __LINE__)
int	__lexer_expect(struct lexer *, enum token_type, struct token **,
    const char *, int);

void	lexer_peek_enter(struct lexer *, struct lexer_state *);
void	lexer_peek_leave(struct lexer *, struct lexer_state *);

int	lexer_peek(struct lexer *, struct token **);

#define LEXER_TYPE_FLAG_CAST	0x00000001u
#define LEXER_TYPE_FLAG_ARG	0x00000002u

int	lexer_peek_if_type(struct lexer *, struct token **, unsigned int);
int	lexer_if_type(struct lexer *, struct token **, unsigned int);

int	lexer_peek_if(struct lexer *, enum token_type, struct token **);
int	lexer_if(struct lexer *, enum token_type, struct token **);

int	lexer_peek_if_flags(struct lexer *, unsigned int, struct token **);
int	lexer_if_flags(struct lexer *, unsigned int, struct token **);

int	lexer_peek_if_pair(struct lexer *, enum token_type, enum token_type,
    struct token **);
int	lexer_if_pair(struct lexer *, enum token_type, enum token_type,
    struct token **);

int	lexer_peek_if_prefix_flags(struct lexer *, unsigned int,
    struct token **);

int	lexer_peek_until(struct lexer *, enum token_type, struct token **);
int	lexer_peek_until_loose(struct lexer *, enum token_type,
    const struct token *, struct token **);
int	lexer_until(struct lexer *, enum token_type, struct token **);

const struct diffchunk	*lexer_get_diffchunk(const struct lexer *,
    unsigned int);

void	lexer_dump(const struct lexer *);
