#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uthash.h>

#include "extern.h"

struct lexer {
	struct lexer_state	 lx_st;
	const char		*lx_path;
	struct buffer		*lx_bf;

	int	lx_error;
	int	lx_eof;
	int	lx_peek;

	struct token_list	lx_tokens;
};

struct token_hash {
	struct token	th_tk;
	UT_hash_handle	th_hh;
};

static int		 lexer_getc(struct lexer *, unsigned char *);
static void		 lexer_ungetc(struct lexer *);
static int		 lexer_read(struct lexer *, struct token **);
static struct token	*lexer_eat_lines(struct lexer *, int);
static struct token	*lexer_eat_space(struct lexer *, int, int);
static struct token	*lexer_ambiguous(struct lexer *);
static struct token	*lexer_comment(struct lexer *, int);
static struct token	*lexer_cpp(struct lexer *);
static void		 lexer_foreach(struct lexer *, struct token *);

static int	lexer_find_token(const struct lexer *,
    const struct lexer_state *, struct token **);
static int	lexer_buffer_strcmp(const struct lexer *,
    const struct lexer_state *, const char *);

static struct token	*lexer_emit(struct lexer *, const struct lexer_state *,
    const struct token *);
static void		 lexer_emit_error(struct lexer *, enum token_type,
    const struct token *, const char *, int);

static int		 isnum(unsigned char, int);
static const char	*strnstr(const char *, size_t, const char *);

static void		 token_free(struct token *);
static const char	*strtoken(enum token_type);

static struct token_hash	*tokens = NULL;

static const struct token	tkcomment = {
	.tk_type	= TOKEN_COMMENT,
	.tk_flags	= TOKEN_FLAG_DANGLING,
};
static const struct token	tkcpp = {
	.tk_type	= TOKEN_CPP,
	.tk_flags	= TOKEN_FLAG_DANGLING,
};
static struct token		tkeof = {
	.tk_type	= TOKEN_EOF,
	.tk_str		= "",
};
static struct token		tkerr = {
	.tk_type	= TOKEN_ERROR,
	.tk_str		= "",
};
static const struct token	tkident = {
	.tk_type	= TOKEN_IDENT,
};
static const struct token	tkline = {
	.tk_type	= TOKEN_SPACE,
	.tk_str		= "\n",
	.tk_len		= 1,
	.tk_flags	= TOKEN_FLAG_DANGLING,
};
static const struct token	tklit = {
	.tk_type	= TOKEN_LITERAL,
};
static const struct token	tkspace = {
	.tk_type	= TOKEN_SPACE,
	.tk_flags	= TOKEN_FLAG_DANGLING,
};
static const struct token	tkstr = {
	.tk_type	= TOKEN_STRING,
};

int
token_cmp(const struct token *t1, const struct token *t2)
{
	if (t1->tk_lno < t2->tk_lno)
		return -1;
	if (t1->tk_lno > t2->tk_lno)
		return 1;
	/* Intentionally not comparing the column. */
	return 0;
}

/*
 * Returns non-zero if the given token has dangling tokens.
 */
int
token_is_dangling(const struct token *tk)
{
	return !TAILQ_EMPTY(&tk->tk_prefixes) || !TAILQ_EMPTY(&tk->tk_suffixes);
}

/*
 * Returns non-zero if the given token has a trailing hard line.
 */
int
token_has_line(const struct token *tk)
{
	const struct token *tmp;

	TAILQ_FOREACH(tmp, &tk->tk_suffixes, tk_entry) {
		if (tmp->tk_type == TOKEN_SPACE)
			return 1;
	}
	return 0;
}

char *
token_sprintf(const struct token *tk)
{
	char *buf = NULL;
	const char *str;
	ssize_t bufsiz = 0;
	int i;

	str = strtoken(tk->tk_type);
	for (i = 0; i < 2; i++) {
		int n;

		n = snprintf(buf, bufsiz, "%s<%u:%u>(\"%.*s\")", str,
		    tk->tk_lno, tk->tk_cno, (int)tk->tk_len, tk->tk_str);
		if (n < 0 || (buf != NULL && n >= bufsiz))
			errc(1, ENAMETOOLONG, "snprintf");
		if (buf == NULL) {
			bufsiz = n + 1;
			buf = malloc(bufsiz);
			if (buf == NULL)
				err(1, NULL);
		}
	}
	return buf;
}

/*
 * Populate the token hash map.
 */
void
lexer_init(void)
{
	static struct token_hash keywords[] = {
#define X(t, s, f) { .th_tk = {						\
	.tk_type	= (t),						\
	.tk_lno		= 0,						\
	.tk_cno		= 0,						\
	.tk_flags	= (f),						\
	.tk_str		= (s),						\
	.tk_len		= sizeof((s)) - 1,				\
	.tk_prefixes	= { NULL, NULL },				\
	.tk_suffixes	= { NULL, NULL },				\
	.tk_entry	= { NULL, NULL }				\
}},
#include "token.h"
#undef X
	};
	unsigned int i;

	for (i = 0; keywords[i].th_tk.tk_type != TOKEN_NONE; i++) {
		struct token_hash *th = &keywords[i];

		if (th->th_tk.tk_len > 0)
			HASH_ADD_KEYPTR(th_hh, tokens, th->th_tk.tk_str,
			    th->th_tk.tk_len, th);
	}
}

struct lexer *
lexer_alloc(const char *path)
{
	struct buffer *bf;
	struct lexer *lx;
	int error = 0;

	bf = buffer_read(path);
	if (bf == NULL)
		return NULL;

	lx = calloc(1, sizeof(*lx));
	if (lx == NULL)
		err(1, NULL);
	lx->lx_bf = bf;
	lx->lx_st.st_lno = 1;
	lx->lx_st.st_cno = 1;
	lx->lx_path = path;
	TAILQ_INIT(&lx->lx_tokens);

	for (;;) {
		struct token *tk;

		if (!lexer_read(lx, &tk)) {
			error = 1;
			break;
		}
		if (tk->tk_type == TOKEN_EOF)
			break;
	}
	if (error) {
		lexer_free(lx);
		return NULL;
	}

	return lx;
}

void
lexer_free(struct lexer *lx)
{
	struct token *tk;

	if (lx == NULL)
		return;

	while ((tk = TAILQ_FIRST(&lx->lx_tokens)) != NULL) {
		TAILQ_REMOVE(&lx->lx_tokens, tk, tk_entry);
		token_free(tk);
	}
	buffer_free(lx->lx_bf);
	free(lx);
}

struct buffer *
lexer_get_buffer(struct lexer *lx)
{
	return lx->lx_bf;
}

int
lexer_get_error(const struct lexer *lx)
{
	return lx->lx_error;
}

int
lexer_pop(struct lexer *lx, struct token **tk)
{
	struct lexer_state *st = &lx->lx_st;

	if (TAILQ_EMPTY(&lx->lx_tokens))
		return 0;

	if (st->st_tok == NULL)
		st->st_tok = TAILQ_FIRST(&lx->lx_tokens);
	else if (st->st_tok->tk_type != TOKEN_EOF)
		st->st_tok = TAILQ_NEXT(st->st_tok, tk_entry);
	if (st->st_tok == NULL)
		return 0;
	*tk = st->st_tok;
	return 1;
}

/*
 * Get the last consumed token. Returns non-zero if such token is found.
 */
int
lexer_back(const struct lexer *lx, struct token **tk)
{
	if (lx->lx_st.st_tok == NULL)
		return 0;
	*tk = lx->lx_st.st_tok;
	return 1;
}

int
__lexer_expect(struct lexer *lx, enum token_type type, struct token **tk,
    const char *fun, int lno)
{
	struct token *t;

	if (!lexer_pop(lx, &t))
		goto err;
	if (t->tk_type == type) {
		if (tk != NULL)
			*tk = t;
		return 1;
	}

err:
	lexer_emit_error(lx, type, t, fun, lno);
	return 0;
}

void
lexer_peek_enter(struct lexer *lx, struct lexer_state *st)
{
	*st = lx->lx_st;
	lx->lx_peek++;
}

void
lexer_peek_leave(struct lexer *lx, struct lexer_state *st)
{
	lx->lx_st = *st;
	assert(lx->lx_peek > 0);
	lx->lx_peek--;
}

/*
 * Peek at the next token without consuming it. Returns non-zero if such token
 * was found.
 */
int
lexer_peek(struct lexer *lx, struct token **tk)
{
	struct lexer_state s;
	struct token *t;
	int pop;

	lexer_peek_enter(lx, &s);
	pop = lexer_pop(lx, &t);
	lexer_peek_leave(lx, &s);
	if (!pop)
		return 0;
	if (tk != NULL)
		*tk = t;
	return 1;
}

int
lexer_peek_func_ptr(struct lexer *lx)
{
	struct lexer_state s;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_LPAREN, NULL) &&
	    lexer_if(lx, TOKEN_STAR, NULL) &&
	    lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if(lx, TOKEN_RPAREN, NULL) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);

	return peek;
}

/*
 * Returns non-zero if the next token(s) denotes a type.
 */
int
lexer_peek_type(struct lexer *lx, struct token **tk, int ispeek)
{
	struct lexer_state s;
	struct token *beg, *t;
	int peek = 0;
	int ntokens = 0;
	int unknown = 0;

	if (!lexer_peek(lx, &beg))
		return 0;
	if (!ispeek)
		lexer_peek_enter(lx, &s);
	for (;;) {
		unsigned int flags = TOKEN_FLAG_TYPE | TOKEN_FLAG_QUALIFIER |
		    TOKEN_FLAG_STORAGE;

		if (lexer_peek_if(lx, TOKEN_EOF, NULL))
			break;

		if (lexer_if_flags(lx, flags, &t)) {
			if (t->tk_flags & TOKEN_FLAG_IDENT)
				lexer_if(lx, TOKEN_IDENT, &t);
			peek = 1;
		} else if (lexer_if(lx, TOKEN_STAR, &t)) {
			/*
			 * A pointer is expected to only be followed by another
			 * pointer or a known type. Otherwise, the following
			 * identifier cannot be part of the type.
			 */
			if (lexer_peek_if(lx, TOKEN_IDENT, NULL))
				break;
			peek = 1;
		} else if (lexer_peek_if(lx, TOKEN_IDENT, NULL)) {
			struct lexer_state ss;
			int ident;

			/*
			 * Recognize function arguments consisting of a single
			 * type and no variable name.
			 */
			ident = 0;
			lexer_peek_enter(lx, &ss);
			if (ntokens == 0 && lexer_if(lx, TOKEN_IDENT, NULL) &&
			    (lexer_if(lx, TOKEN_RPAREN, NULL) ||
			     lexer_if(lx, TOKEN_COMMA, NULL)))
				ident = 1;
			lexer_peek_leave(lx, &ss);
			if (ident) {
				if (lexer_pop(lx, &t))
					peek = 1;
				break;
			}

			/* Recognize function pointers. */
			ident = 0;
			lexer_peek_enter(lx, &ss);
			if (lexer_if(lx, TOKEN_IDENT, NULL) &&
			    lexer_peek_func_ptr(lx))
				ident = 1;
			lexer_peek_leave(lx, &ss);
			if (ident) {
				if (lexer_pop(lx, &t))
					peek = 1;
				break;
			}

			/*
			 * Preprocessor macros can be intertwined with a type,
			 * such macros are recognized as an identifier by the
			 * lexer.
			 */
			ident = 1;
			lexer_peek_enter(lx, &ss);
			if (lexer_if(lx, TOKEN_IDENT, NULL) &&
			    (lexer_if_flags(lx, TOKEN_FLAG_ASSIGN, NULL) ||
			     lexer_if(lx, TOKEN_LSQUARE, NULL) ||
			     lexer_if(lx, TOKEN_LPAREN, NULL) ||
			     lexer_if(lx, TOKEN_RPAREN, NULL) ||
			     lexer_if(lx, TOKEN_SEMI, NULL) ||
			     lexer_if(lx, TOKEN_COMMA, NULL) ||
			     lexer_if(lx, TOKEN_COLON, NULL) ||
			     lexer_if(lx, TOKEN_ATTRIBUTE, NULL)))
				ident = 0;
			lexer_peek_leave(lx, &ss);
			if (!ident)
				break;

			/* Consume the identifier, i.e. preprocessor macro. */
			lexer_if(lx, TOKEN_IDENT, &t);
		} else {
			unknown = 1;
			break;
		}

		ntokens++;
	}
	if (!ispeek)
		lexer_peek_leave(lx, &s);

	if (ntokens == 1 &&
	    (beg->tk_flags & (TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE))) {
		/* A single qualifier or storage token cannot denote a type. */
		peek = 0;
	} else if (!peek && !unknown && ntokens > 0) {
		/*
		 * Nothing was found. However this is a sequence of identifiers (i.e.
		 * unknown types) therefore treat it as a type.
		 */
		peek = 1;
	}

	if (peek && tk != NULL)
		*tk = t;
	return peek;
}

/*
 * Peek at the next token without consuming it only if it matches the given
 * type. Returns non-zero if such token was found.
 */
int
lexer_peek_if(struct lexer *lx, enum token_type type, struct token **tk)
{
	struct token *t;

	if (lexer_peek(lx, &t) && t->tk_type == type) {
		if (tk != NULL)
			*tk = t;
		return 1;
	}
	return 0;
}

/*
 * Consume the next token if it matches the given type. Returns non-zero if such
 * token was found.
 */
int
lexer_if(struct lexer *lx, enum token_type type, struct token **tk)
{
	struct token *t;

	if (!lexer_peek_if(lx, type, tk) || !lexer_pop(lx, &t))
		return 0;
	return 1;
}

/*
 * Peek at the flags of the next token without consuming it. Returns non-zero if
 * such token was found.
 */
int
lexer_peek_if_flags(struct lexer *lx, unsigned int flags, struct token **tk)
{
	struct lexer_state s;
	int peek;

	lexer_peek_enter(lx, &s);
	peek = lexer_if_flags(lx, flags, tk);
	lexer_peek_leave(lx, &s);
	return peek;
}

/*
 * Consume the next token if it matches the given flags. Returns non-zero such
 * token was found.
 */
int
lexer_if_flags(struct lexer *lx, unsigned int flags, struct token **tk)
{
	struct lexer_state s;
	struct token *t;
	int error = 0;

	lexer_peek_enter(lx, &s);
	if (!lexer_pop(lx, &t) || (t->tk_flags & flags) == 0)
		error = 1;
	lexer_peek_leave(lx, &s);
	if (error || !lexer_pop(lx, &t))
		return 0;
	if (tk != NULL)
		*tk = t;
	return 1;
}

/*
 * Peek at the next balanced pair of tokens such as parenthesis or squares.
 * Returns non-zero if such tokens was found.
 */
int
lexer_peek_if_pair(struct lexer *lx, enum token_type lhs, enum token_type rhs,
    struct token **tk)
{
	struct lexer_state s;
	struct token *t;
	int pair = 0;

	if (!lexer_peek_if(lx, lhs, NULL))
		return 0;

	lexer_peek_enter(lx, &s);
	for (;;) {
		if (!lexer_pop(lx, &t))
			break;
		if (t->tk_type == TOKEN_EOF)
			break;
		if (t->tk_type == lhs)
			pair++;
		if (t->tk_type == rhs)
			pair--;
		if (pair == 0)
			break;
	}
	lexer_peek_leave(lx, &s);

	if (lx->lx_error == 0 && pair == 0) {
		if (tk != NULL)
			*tk = t;
		return 1;
	}
	return 0;
}

/*
 * Consume the next balanced pair of tokens such as parenthesis or squares.
 * Returns non-zero if such tokens was found.
 */
int
lexer_if_pair(struct lexer *lx, enum token_type lhs, enum token_type rhs,
    struct token **tk)
{
	struct token *end;

	if (!lexer_peek_if_pair(lx, lhs, rhs, &end))
		return 0;

	lx->lx_st.st_tok = end;
	if (tk != NULL)
		*tk = end;
	return 1;
}

/*
 * Peek until the given token type is encountered. Returns non-zero if such
 * token was found.
 */
int
lexer_peek_until(struct lexer *lx, enum token_type type, struct token **tk)
{
	struct lexer_state s;
	int peek;

	lexer_peek_enter(lx, &s);
	peek = lexer_until(lx, type, tk);
	lexer_peek_leave(lx, &s);
	return peek;
}

/*
 * Peek until the given token type is encountered and it is not nested under any
 * pairs of parenthesis nor braces but halt while trying to move beyond the
 * given stop token. Returns non-zero if such token was found.
 */
int
lexer_peek_until_loose(struct lexer *lx, enum token_type type,
    const struct token *stop, struct token **tk)
{
	struct lexer_state s;
	struct token *t;
	int nest = 0;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	for (;;) {
		if (!lexer_pop(lx, &t) || t == stop || t->tk_type == TOKEN_EOF)
			break;
		if (t->tk_type == type && !nest) {
			peek = 1;
			break;
		}
		if (t->tk_type == TOKEN_LPAREN || t->tk_type == TOKEN_LBRACE)
			nest++;
		else if (t->tk_type == TOKEN_RPAREN ||
		    t->tk_type == TOKEN_RBRACE)
			nest--;
	}
	lexer_peek_leave(lx, &s);
	if (peek && tk != NULL)
		*tk = t;
	return peek;
}

/*
 * Peek until the given token type is encountered but abort while trying to move
 * beyond the given stop token. Returns non-zero if such token was found.
 */
int
lexer_peek_until_stop(struct lexer *lx, enum token_type type,
    const struct token *stop, struct token **tk)
{
	struct lexer_state s;
	int peek;

	lexer_peek_enter(lx, &s);
	peek = __lexer_until(lx, type, stop, tk, __func__, __LINE__);
	lexer_peek_leave(lx, &s);
	return peek;
}

/*
 * Consume token(s) until the given token type is encountered but abort while
 * trying to move beyond the given stop token. Returns non-zero if such token is
 * found.
 */
int
__lexer_until(struct lexer *lx, enum token_type type, const struct token *stop,
    struct token **tk, const char *fun, int lno)
{
	for (;;) {
		struct token *t;

		if (!lexer_pop(lx, &t) || t->tk_type == TOKEN_EOF ||
		    (stop != NULL && stop == t)) {
			lexer_emit_error(lx, type, t, fun, lno);
			return 0;
		}
		if (t->tk_type == type) {
			if (tk != NULL)
				*tk = t;
			return 1;
		}
	}
	return 0;
}

static int
lexer_getc(struct lexer *lx, unsigned char *ch)
{
	struct buffer *bf = lx->lx_bf;
	unsigned char c;

	if (lx->lx_st.st_off == bf->bf_len) {
		/*
		 * Do not immediately report EOF. Instead, return something
		 * that's not expected while reading a token.
		 */
		if (lx->lx_eof++ > 0)
			return 1;
		*ch = '\0';
		return 0;
	}
	c = bf->bf_ptr[lx->lx_st.st_off++];
	if (c == '\n') {
		lx->lx_st.st_lno++;
		lx->lx_st.st_cno = 1;
	} else {
		lx->lx_st.st_cno++;
	}
	*ch = c;
	return 0;
}

static void
lexer_ungetc(struct lexer *lx)
{
	if (lx->lx_eof)
		return;

	assert(lx->lx_st.st_off > 0);
	lx->lx_st.st_off--;

	if (lx->lx_bf->bf_ptr[lx->lx_st.st_off] == '\n') {
		assert(lx->lx_st.st_lno > 0);
		lx->lx_st.st_lno--;
		lx->lx_st.st_cno = 1;
	} else {
		assert(lx->lx_st.st_cno > 0);
		lx->lx_st.st_cno--;
	}
}

/*
 * Consume the next token. Returns non-zero if such token is found.
 */
static int
lexer_read(struct lexer *lx, struct token **tk)
{
	struct lexer_state st;
	struct token_list dangling;
	struct token *tmp;
	struct token *t;
	int error = 0;
	unsigned char ch;

	TAILQ_INIT(&dangling);
	st = lx->lx_st;

	if (lx->lx_error)
		goto err;

	/*
	 * Consume all comments and preprocessor directives, will be hanging of
	 * the emitted token.
	 */
	for (;;) {
		if ((tmp = lexer_comment(lx, 1)) != NULL ||
		    (tmp = lexer_cpp(lx)) != NULL)
			TAILQ_INSERT_TAIL(&dangling, tmp, tk_entry);
		else
			break;
	}

	lexer_eat_space(lx, 1, 0);

	if ((*tk = lexer_ambiguous(lx)) != NULL)
		goto out;

	st = lx->lx_st;
	if (lexer_getc(lx, &ch))
		goto eof;

	if (ch == 'L') {
		unsigned char peek;

		if (lexer_getc(lx, &peek) == 0 && (peek == '"' || peek == '\''))
			ch = peek;
		else
			lexer_ungetc(lx);
	}
	if (ch == '"' || ch == '\'') {
		unsigned char delim = ch;
		unsigned char pch = ch;

		for (;;) {
			if (lexer_getc(lx, &ch))
				goto eof;
			if (pch == '\\' && ch == '\\')
				ch = '\0';
			else if (pch != '\\' && ch == delim)
				break;
			pch = ch;
		}
		*tk = lexer_emit(lx, &st, delim == '"' ? &tkstr : &tklit);
		goto out;
	}

	if (isnum(ch, 1)) {
		do {
			if (lexer_getc(lx, &ch))
				goto eof;
		} while (isnum(ch, 0));
		lexer_ungetc(lx);
		*tk = lexer_emit(lx, &st, &tklit);
		goto out;
	}

	if (isalpha(ch) || ch == '_') {
		while (isalnum(ch) || ch == '_') {
			if (lexer_getc(lx, &ch))
				goto eof;
		}
		lexer_ungetc(lx);

		if (lexer_find_token(lx, &st, &t)) {
			*tk = lexer_emit(lx, &st, t);
			goto out;
		}
	}

	/* Fallback, treat everything as an identifier. */
	*tk = lexer_emit(lx, &st, &tkident);
	goto out;

err:
	*tk = lexer_emit(lx, &st, &tkerr);
	error = 1;
	goto out;

eof:
	*tk = lexer_emit(lx, &st, &tkeof);

out:
	TAILQ_CONCAT(&(*tk)->tk_prefixes, &dangling, tk_entry);

	/*
	 * Consume trailing/interwined comments, will be hanging of the emitted
	 * token.
	 */
	for (;;) {
		if ((tmp = lexer_comment(lx, 0)) == NULL)
			break;
		TAILQ_INSERT_TAIL(&(*tk)->tk_suffixes, tmp, tk_entry);
	}

	/* Consume hard lines, will be hanging of the emitted token. */
	if ((tmp = lexer_eat_lines(lx, 1)) != NULL)
		TAILQ_INSERT_TAIL(&(*tk)->tk_suffixes, tmp, tk_entry);

	lexer_foreach(lx, *tk);

	return error ? 0 : 1;
}

static struct token *
lexer_eat_lines(struct lexer *lx, int emit)
{
	struct lexer_state st;
	int nlines = 0;
	unsigned char ch;

	st = lx->lx_st;

	for (;;) {
		if (lexer_getc(lx, &ch))
			break;
		if (ch == '\n') {
			nlines++;
		} else {
			lexer_ungetc(lx);
			break;
		}
	}
	if (nlines <= 1)
		return NULL;
	return emit ? lexer_emit(lx, &st, &tkline) : NULL;
}

static struct token *
lexer_eat_space(struct lexer *lx, int newline, int emit)
{
	struct lexer_state st;
	unsigned char ch;

	st = lx->lx_st;

	do {
		if (lexer_getc(lx, &ch))
			return NULL;
	} while (ch == ' ' || ch == '\t' || (ch == '\n' && newline));
	lexer_ungetc(lx);

	if (!emit || st.st_off == lx->lx_st.st_off)
		return NULL;
	return lexer_emit(lx, &st, &tkspace);
}

static struct token *
lexer_ambiguous(struct lexer *lx)
{
	struct lexer_state st = lx->lx_st;
	struct token *pv = NULL;
	struct token *tk = NULL;
	unsigned char ch;

	if (lexer_getc(lx, &ch))
		return NULL;

	for (;;) {
		struct token *tmp;

		if (!lexer_find_token(lx, &st, &tmp)) {
			if (pv != NULL)
				lexer_ungetc(lx);
			tk = pv;
			break;
		}
		if ((tmp->tk_flags & TOKEN_FLAG_AMBIGUOUS) == 0) {
			tk = tmp;
			break;
		}

		pv = tmp;
		if (lexer_getc(lx, &ch)) {
			tk = tmp;
			break;
		}
	}
	if (tk == NULL) {
		lx->lx_st = st;
		return NULL;
	}

	return lexer_emit(lx, &st, tk);
}

static struct token *
lexer_comment(struct lexer *lx, int block)
{
	struct lexer_state oldst, st;
	int ncomments = 0;

	/* Stamp the state which marks the start of comments. */
	st = lx->lx_st;

	for (;;) {
		int cstyle;
		unsigned char ch;

		/*
		 * Stamp the state before consuming whitespace as peeking must
		 * not cause any side effects.
		 */
		oldst = lx->lx_st;

		lexer_eat_space(lx, block, 0);

		if (lexer_getc(lx, &ch) || ch != '/') {
			lx->lx_st = oldst;
			break;
		}
		if (lexer_getc(lx, &ch) || (ch != '/' && ch != '*')) {
			lx->lx_st = oldst;
			break;
		}
		cstyle = ch == '*';

		ch = '\0';
		for (;;) {
			unsigned char peek;

			if (lexer_getc(lx, &peek))
				break;
			if (cstyle) {
				if (ch == '*' && peek == '/')
					break;
				ch = peek;
			} else {
				if (peek == '\n') {
					lexer_ungetc(lx);
					break;
				}
			}
		}

		ncomments++;
		if (!block)
			break;
	}
	if (ncomments == 0)
		return NULL;

	/*
	 * Optionally consume trailing whitespace and hard lines(s), will be
	 * hanging of the comment token. This is only relevant for block
	 * comments.
	 */
	if (block) {
		(void)lexer_eat_space(lx, 0, 0);
		(void)lexer_eat_lines(lx, 0);
	}

	return lexer_emit(lx, &st, &tkcomment);
}

static struct token *
lexer_cpp(struct lexer *lx)
{
	struct lexer_state st;
	int ncpp = 0;
	int off = 0;

	/* Stamp the state which marks the start of preprocessor directives. */
	st = lx->lx_st;

	for (;;) {
		struct lexer_state cppst, oldst;
		unsigned char ch;
		int comment;

		/*
		 * Stamp the state before consuming whitespace as peeking must
		 * not cause any side effects.
		 */
		oldst = lx->lx_st;

		lexer_eat_space(lx, 1, 0);

		/*
		 * Stamp the state after consuming whitespace in order to
		 * capture the complete line representing a preprocessor
		 * directive. Used to check for presence of disabled blocks
		 * below.
		 */
		cppst = lx->lx_st;

		if (lexer_getc(lx, &ch) || (ch != '#' && !off)) {
			lx->lx_st = oldst;
			break;
		}

		ch = '\0';
		comment = 0;
		for (;;) {
			unsigned char peek;

			if (lexer_getc(lx, &peek))
				break;

			/*
			 * Make block comments part of the preprocessor
			 * directive.
			 */
			if (ch == '/' && peek == '*')
				comment = 1;
			else if (comment && ch == '*' && peek == '/')
				comment = 0;
			else if (!comment && ch != '\\' && peek == '\n')
				break;
			ch = peek;
		}

		/* Treat disabled blocks as verbatim. */
		if (off) {
			if (lexer_buffer_strcmp(lx, &cppst, "#if"))
				off++;
			else if (lexer_buffer_strcmp(lx, &cppst, "#endif"))
				off--;
		} else {
			if (lexer_buffer_strcmp(lx, &cppst, "#if 0") ||
			    lexer_buffer_strcmp(lx, &cppst, "#ifdef notyet"))
				off++;
		}

		ncpp++;
	}
	if (ncpp == 0)
		return NULL;

	/* Consume hard line(s), will be hanging of the cpp token. */
	(void)lexer_eat_lines(lx, 0);

	return lexer_emit(lx, &st, &tkcpp);
}

/*
 * Detect foreach like constructs such as the ones provided by queue(3).
 */
static void
lexer_foreach(struct lexer *lx, struct token *tk)
{
	unsigned char ch;

	if (strnstr(tk->tk_str, tk->tk_len, "FOREACH") == NULL &&
	    strnstr(tk->tk_str, tk->tk_len, "_for_each") == NULL &&
	    strnstr(tk->tk_str, tk->tk_len, "for_each_") == NULL)
		return;

	if (lexer_getc(lx, &ch))
		return;
	lexer_ungetc(lx);
	if (ch == '(')
		tk->tk_type = TOKEN_FOREACH;
}

static int
lexer_find_token(const struct lexer *lx, const struct lexer_state *st,
    struct token **tk)
{
	struct token_hash *th;
	const char *key;
	size_t len;

	len = lx->lx_st.st_off - st->st_off;
	key = &lx->lx_bf->bf_ptr[st->st_off];
	HASH_FIND(th_hh, tokens, key, len, th);
	if (th == NULL)
		return 0;
	*tk = &th->th_tk;
	return 1;
}

static int
lexer_buffer_strcmp(const struct lexer *lx, const struct lexer_state *st,
    const char *str)
{
	const char *buf;
	size_t buflen, len;

	buf = &lx->lx_bf->bf_ptr[st->st_off];
	buflen = lx->lx_st.st_off - st->st_off;
	len = strlen(str);
	if (len > buflen)
		return 0;
	return strncmp(buf, str, len) == 0 ? 1 : 0;
}

static struct token *
lexer_emit(struct lexer *lx, const struct lexer_state *st,
    const struct token *tk)
{
	struct token *t;

	t = calloc(1, sizeof(*t));
	if (t == NULL)
		err(1, NULL);
	*t = *tk;
	t->tk_lno = st->st_lno;
	t->tk_cno = st->st_cno;
	if (t->tk_str == NULL) {
		t->tk_str = &lx->lx_bf->bf_ptr[st->st_off];
		t->tk_len = lx->lx_st.st_off - st->st_off;
	}
	if ((t->tk_flags & TOKEN_FLAG_DANGLING) == 0)
		TAILQ_INSERT_TAIL(&lx->lx_tokens, t, tk_entry);
	TAILQ_INIT(&t->tk_prefixes);
	TAILQ_INIT(&t->tk_suffixes);
	return t;
}

static void
lexer_emit_error(struct lexer *lx, enum token_type type,
    const struct token *tk, const char *fun, int lno)
{
	char *str;

	/* Be quiet while peeking. */
	if (lx->lx_peek > 0)
		return;

	/* Be quiet if an error already has been emitted. */
	if (lx->lx_error++ > 0)
		return;

	str = token_sprintf(tk);
	fprintf(stderr, "%s:%d: expected type %s got %s\n", fun, lno,
	    strtoken(type), str);
	free(str);
}

static int
isnum(unsigned char ch, int prefix)
{
	if (prefix)
		return isdigit(ch);
	return isdigit(ch) || isxdigit(ch) || ch == 'l' || ch == 'L' ||
	    ch == 'x' || ch == 'X' || ch == 'u' || ch == 'U';
}

static const char *
strnstr(const char *big, size_t biglen, const char *little)
{
	size_t i, j;

	for (i = 0; i < biglen; i++) {
		const char *s = little;

		if (big[i] != s[0])
			continue;

		for (j = i; j < biglen; j++) {
			if (big[j] != s[0])
				break;
			if (*++s == '\0')
				return &big[i];
		}
	}

	return NULL;
}

static void
token_free(struct token *tk)
{
	struct token *tmp;

	if (tk == NULL)
		return;

	while ((tmp = TAILQ_FIRST(&tk->tk_prefixes)) != NULL) {
		TAILQ_REMOVE(&tk->tk_prefixes, tmp, tk_entry);
		token_free(tmp);
	}
	while ((tmp = TAILQ_FIRST(&tk->tk_suffixes)) != NULL) {
		TAILQ_REMOVE(&tk->tk_suffixes, tmp, tk_entry);
		token_free(tmp);
	}
	free(tk);
}

static const char *
strtoken(enum token_type type)
{
	switch (type) {
#define X(t, s, f) case t: return &#t[sizeof("TOKEN_") - 1];
#include "token.h"
#undef X
	}
	return NULL;
}
