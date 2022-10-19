#include "lexer.h"

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>

#include "buffer.h"
#include "cdefs.h"
#include "diff.h"
#include "error.h"
#include "options.h"
#include "token.h"
#include "util.h"
#include "vector.h"

#ifdef HAVE_UTHASH
#  include <uthash.h>
#else
#  include "compat-uthash.h"
#endif

struct lexer {
	struct lexer_state	 lx_st;
	struct error		*lx_er;
	const struct options	*lx_op;
	const struct diffchunk	*lx_diff;
	const struct buffer	*lx_bf;
	const char		*lx_path;

	/* Serialize callback passed to lexer_alloc(). */
	char			*(*lx_serialize)(const struct token *);

	/* Line number to buffer offset mapping. */
	VECTOR(unsigned int)	 lx_lines;

	int			 lx_eof;
	int			 lx_peek;

	struct token		*lx_unmute;

	struct token_list	 lx_tokens;
	VECTOR(struct token *)	 lx_stamps;
	VECTOR(char *)		 lx_serialized;
};

struct token_hash {
	struct token	th_tk;
	UT_hash_handle	th_hh;
};

static void		 lexer_eat_buffet(struct lexer *, struct lexer_state *,
    struct lexer_state *);
static int		 lexer_eat_lines(struct lexer *, struct token **, int);
static int		 lexer_eat_spaces(struct lexer *, struct token **);
static struct token	*lexer_keyword(struct lexer *);
static struct token	*lexer_keyword1(struct lexer *);
static struct token	*lexer_comment(struct lexer *, int);
static struct token	*lexer_cpp(struct lexer *);
static struct token	*lexer_ellipsis(struct lexer *,
    const struct lexer_state *);
static int		 lexer_eof(const struct lexer *);

static void	lexer_line_alloc(struct lexer *, unsigned int);

static struct token	*lexer_find_token(const struct lexer *,
    const struct lexer_state *);
static int		 lexer_buffer_streq(const struct lexer *,
    const struct lexer_state *, const char *);

static void	lexer_emit_error(struct lexer *, int, const struct token *,
    const char *, int);

static int	lexer_peek_if_func_ptr(struct lexer *, struct token **);
static int	lexer_peek_if_ptr(struct lexer *, struct token **);
static int	lexer_peek_if_type_ident(struct lexer *lx);
static int	lexer_peek_if_type_cpp(struct lexer *lx);

static void	lexer_branch_fold(struct lexer *, struct token *);
static void	lexer_branch_unmute(struct lexer *, struct token *);

#define LEXER_BRANCH_BACKWARD		0x00000001u
#define LEXER_BRANCH_FORWARD		0x00000002u
#define LEXER_BRANCH_INTACT		0x00000004u

static struct token	*lexer_recover_branch(struct token *);
static struct token	*lexer_recover_branch1(struct token *, unsigned int);

#define lexer_trace(lx, fmt, ...) do {					\
	if (trace((lx)->lx_op, 'l') >= 2)				\
		lexer_trace0((lx), __func__, (fmt),			\
		    __VA_ARGS__);					\
} while (0)
static void	lexer_trace0(const struct lexer *, const char *, const char *,
    ...)
	__attribute__((__format__(printf, 3, 4)));

static void	token_prolong(struct token *, struct token *);

static int	isnum(unsigned char, int);

static struct token_hash	*tokens = NULL;

static const struct token tkcomment = {
	.tk_type	= TOKEN_COMMENT,
};
static const struct token tkcpp = {
	.tk_type	= TOKEN_CPP,
	.tk_flags	= TOKEN_FLAG_CPP,
};
static struct token tkeof = {
	.tk_type	= LEXER_EOF,
	.tk_str		= "",
};
static const struct token tkident = {
	.tk_type	= TOKEN_IDENT,
};
static const struct token tkline = {
	.tk_type	= TOKEN_SPACE,
	.tk_str		= "\n",
	.tk_len		= 1,
};
static const struct token tklit = {
	.tk_type	= TOKEN_LITERAL,
};
static const struct token tknone = {
	.tk_type	= TOKEN_NONE,
};
static const struct token tkspace = {
	.tk_type	= TOKEN_SPACE,
};
static const struct token tkstr = {
	.tk_type	= TOKEN_STRING,
};

/*
 * Populate the token hash map.
 */
void
lexer_init(void)
{
	static struct token_hash keywords[] = {
#define T(t, s, f) { .th_tk = {						\
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
#define A(t, s, f) T(t, s, f)
#include "token-defs.h"
	};
	unsigned int i;

	for (i = 0; keywords[i].th_tk.tk_type != TOKEN_NONE; i++) {
		struct token_hash *th = &keywords[i];
		const struct token *tk = &th->th_tk;

		HASH_ADD_KEYPTR(th_hh, tokens, tk->tk_str, tk->tk_len, th);
	}
}

void
lexer_shutdown(void)
{
	HASH_CLEAR(th_hh, tokens);
}

struct lexer *
lexer_alloc(const struct lexer_arg *arg)
{
	struct lexer *lx;
	int error = 0;

	lx = calloc(1, sizeof(*lx));
	if (lx == NULL)
		err(1, NULL);
	lx->lx_er = arg->er;
	lx->lx_op = arg->op;
	lx->lx_bf = arg->bf;
	lx->lx_diff = arg->diff;
	lx->lx_path = arg->path;
	lx->lx_serialize = arg->callbacks.serialize ?
	    arg->callbacks.serialize : token_sprintf;
	lx->lx_st.st_lno = 1;
	lx->lx_st.st_cno = 1;
	if (VECTOR_INIT(lx->lx_lines) == NULL)
		err(1, NULL);
	TAILQ_INIT(&lx->lx_tokens);
	if (VECTOR_INIT(lx->lx_stamps) == NULL)
		err(1, NULL);
	if (VECTOR_INIT(lx->lx_serialized) == NULL)
		err(1, NULL);
	lexer_line_alloc(lx, 1);

	for (;;) {
		struct token *tk;

		tk = arg->callbacks.read(lx, arg->callbacks.arg);
		if (tk == NULL) {
			error = 1;
			break;
		}
		TAILQ_INSERT_TAIL(&lx->lx_tokens, tk, tk_entry);
		if (tk->tk_type == LEXER_EOF)
			break;
	}

	if (error) {
		lexer_free(lx);
		return NULL;
	}

	if (trace(lx->lx_op, 'l') >= 3)
		lexer_dump(lx);

	return lx;
}

void
lexer_free(struct lexer *lx)
{
	struct token *tk;

	if (lx == NULL)
		return;

	VECTOR_FREE(lx->lx_lines);
	if (lx->lx_unmute != NULL)
		token_rele(lx->lx_unmute);
	while (!VECTOR_EMPTY(lx->lx_stamps)) {
		tk = *VECTOR_POP(lx->lx_stamps);
		token_rele(tk);
	}
	VECTOR_FREE(lx->lx_stamps);
	while ((tk = TAILQ_FIRST(&lx->lx_tokens)) != NULL) {
		TAILQ_REMOVE(&lx->lx_tokens, tk, tk_entry);
		assert(tk->tk_refs == 1);
		token_rele(tk);
	}
	while (!VECTOR_EMPTY(lx->lx_serialized)) {
		char *str;

		str = *VECTOR_POP(lx->lx_serialized);
		free(str);
	}
	VECTOR_FREE(lx->lx_serialized);
	free(lx);
}

void
lexer_get_state(const struct lexer *lx, struct lexer_state *st)
{
	*st = lx->lx_st;
}

int
lexer_getc(struct lexer *lx, unsigned char *ch)
{
	const struct buffer *bf = lx->lx_bf;
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
		lexer_line_alloc(lx, lx->lx_st.st_lno);
	} else {
		lx->lx_st.st_cno++;
	}
	*ch = c;
	return 0;
}

void
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

struct token *
lexer_emit(struct lexer *lx, const struct lexer_state *st,
    const struct token *tk)
{
	struct token *t;

	t = token_alloc(tk);
	t->tk_off = st->st_off;
	t->tk_lno = st->st_lno;
	t->tk_cno = st->st_cno;
	if (lexer_get_diffchunk(lx, t->tk_lno) != NULL)
		t->tk_flags |= TOKEN_FLAG_DIFF;
	if (t->tk_str == NULL) {
		t->tk_str = &lx->lx_bf->bf_ptr[st->st_off];
		t->tk_len = lx->lx_st.st_off - st->st_off;
	}
	return t;
}

void
lexer_error(struct lexer *lx, const char *fmt, ...)
{
	va_list ap;
	struct buffer *bf;

	lx->lx_st.st_err++;

	va_start(ap, fmt);
	bf = error_begin(lx->lx_er);
	buffer_printf(bf, "%s: ", lx->lx_path);
	buffer_vprintf(bf, fmt, ap);
	buffer_printf(bf, "\n");
	error_end(lx->lx_er);
	va_end(ap);
}

/*
 * Serialize the given token. The returned string will be freed once
 * lexer_free() is invoked.
 */
const char *
lexer_serialize(struct lexer *lx, const struct token *tk)
{
	char *str;

	str = lx->lx_serialize(tk);
	*VECTOR_ALLOC(lx->lx_serialized) = str;
	return str;
}

int
lexer_get_error(const struct lexer *lx)
{
	return lx->lx_st.st_err;
}

/*
 * Get the buffer contents for the lines [beg, end). If end is equal to 0, the
 * line number of the last line is used.
 */
int
lexer_get_lines(const struct lexer *lx, unsigned int beg, unsigned int end,
    const char **str, size_t *len)
{
	const struct buffer *bf = lx->lx_bf;
	size_t bo, eo;

	bo = lx->lx_lines[beg - 1];
	if (end == 0)
		eo = bf->bf_len;
	else
		eo = lx->lx_lines[end - 1];
	*str = &bf->bf_ptr[bo];
	*len = eo - bo;
	return 1;
}

/*
 * Take note of the last consumed token, later used while branching and
 * recovering.
 */
void
lexer_stamp(struct lexer *lx)
{
	struct token *tk;

	tk = lx->lx_st.st_tk;
	if (tk->tk_flags & TOKEN_FLAG_STAMP)
		return;

	lexer_trace(lx, "stamp %s", lexer_serialize(lx, tk));
	tk->tk_flags |= TOKEN_FLAG_STAMP;
	token_ref(tk);
	*VECTOR_ALLOC(lx->lx_stamps) = tk;
}

/*
 * Try to recover after encountering invalid source code. Returns the index of
 * the stamped token seeked to, starting from the end. This index should
 * correspond to the number of documents that must be removed since we're about
 * to parse them again.
 */
int
lexer_recover(struct lexer *lx)
{
	struct token *seek = NULL;
	struct token *back, *br, *dst, *src;
	size_t i;
	int ndocs = 1;

	if (!lexer_back(lx, &back))
		back = TAILQ_FIRST(&lx->lx_tokens);
	lexer_trace(lx, "back %s", lexer_serialize(lx, back));
	br = lexer_recover_branch(back);
	if (br == NULL)
		return 0;

	src = br->tk_token;
	dst = br->tk_branch.br_nx->tk_token;
	lexer_trace(lx, "branch from %s to %s covering [%s, %s)",
	    lexer_serialize(lx, br),
	    lexer_serialize(lx, br->tk_branch.br_nx),
	    lexer_serialize(lx, src),
	    lexer_serialize(lx, dst));

	/*
	 * Find the offset of the first stamped token before the branch.
	 * Must be done before getting rid of the branch as stamped tokens might
	 * be removed.
	 */
	for (i = VECTOR_LENGTH(lx->lx_stamps); i > 0; i--) {
		struct token *stamp = lx->lx_stamps[i - 1];

		if (token_cmp(stamp, br) < 0)
			break;
		ndocs++;
	}

	/*
	 * Turn the whole branch into a prefix. As the branch is about to be
	 * removed, grab a reference since it's needed below.
	 */
	token_ref(br);
	lexer_branch_fold(lx, br);

	/* Find first stamped token before the branch. */
	for (i = VECTOR_LENGTH(lx->lx_stamps); i > 0; i--) {
		struct token *stamp = lx->lx_stamps[i - 1];

		if (token_cmp(stamp, br) < 0) {
			seek = stamp;
			break;
		}
	}
	token_rele(br);

	lexer_trace(lx, "seek to %s, removing %d document(s)",
	    lexer_serialize(lx, seek ? seek : TAILQ_FIRST(&lx->lx_tokens)),
	    ndocs);
	lx->lx_st.st_tk = seek;
	lx->lx_st.st_err = 0;
	return ndocs;
}

/*
 * Returns non-zero if the lexer took the next branch.
 */
int
lexer_branch(struct lexer *lx)
{
	struct token *br, *dst, *rm, *seek, *tk;

	if (!lexer_back(lx, &tk))
		return 0;
	br = token_get_branch(tk);
	if (br == NULL)
		return 0;

	dst = br->tk_branch.br_nx->tk_token;

	lexer_trace(lx, "branch from %s to %s, covering [%s, %s)",
	    lexer_serialize(lx, br),
	    lexer_serialize(lx, br->tk_branch.br_nx),
	    lexer_serialize(lx, br->tk_token),
	    lexer_serialize(lx, br->tk_branch.br_nx->tk_token));

	token_branch_unlink(br);

	rm = br->tk_token;
	for (;;) {
		struct token *nx;

		lexer_trace(lx, "removing %s", lexer_serialize(lx, rm));

		nx = token_next(rm);
		lexer_remove(lx, rm, 0);
		if (nx == dst)
			break;
		rm = nx;
	}

	/*
	 * Tell doc_token() that crossing this token must cause tokens to be
	 * emitted again. While here, disarm any previous unmute token as it
	 * might be crossed again.
	 */
	if (lx->lx_unmute != NULL) {
		lx->lx_unmute->tk_flags &= ~TOKEN_FLAG_UNMUTE;
		token_rele(lx->lx_unmute);
		lx->lx_unmute = NULL;
	}
	lexer_branch_unmute(lx, dst);

	/* Rewind to last stamped token. */
	seek = VECTOR_EMPTY(lx->lx_stamps) ? NULL : *VECTOR_LAST(lx->lx_stamps);
	lexer_trace(lx, "seek to %s",
	    lexer_serialize(lx, seek ? seek : TAILQ_FIRST(&lx->lx_tokens)));
	lx->lx_st.st_tk = seek;
	lx->lx_st.st_err = 0;
	return 1;
}

/*
 * Returns non-zero if the current token denotes a branch continuation.
 */
int
lexer_is_branch(const struct lexer *lx)
{
	struct token *tk;

	return lexer_back(lx, &tk) && token_get_branch(tk) != NULL;
}

int
lexer_pop(struct lexer *lx, struct token **tk)
{
	struct lexer_state *st = &lx->lx_st;

	if (st->st_tk == NULL) {
		st->st_tk = TAILQ_FIRST(&lx->lx_tokens);
	} else if (st->st_tk->tk_type != LEXER_EOF) {
		struct token *br;

		/* Do not move passed a branch. */
		if (lx->lx_peek == 0 && token_is_branch(st->st_tk))
			return 0;

		st->st_tk = token_next(st->st_tk);
		if (st->st_tk == NULL)
			return 1;
		br = token_get_branch(st->st_tk);
		if (br == NULL)
			goto out;

		if (lx->lx_peek == 0) {
			/* While not peeking, instruct the parser to halt. */
			lexer_trace(lx, "halt %s",
			    lexer_serialize(lx, st->st_tk));
			return 0;
		} else {
			/* While peeking, act as taking the current branch. */
			while (br->tk_branch.br_nx != NULL)
				br = br->tk_branch.br_nx;
			st->st_tk = br->tk_token;
		}
	}

out:
	*tk = st->st_tk;
	return 1;
}

/*
 * Get the last consumed token. Returns non-zero if such token is found.
 */
int
lexer_back(const struct lexer *lx, struct token **tk)
{
	if (lx->lx_st.st_tk == NULL)
		return 0;
	*tk = lx->lx_st.st_tk;
	return 1;
}

struct token *
lexer_copy_after(struct lexer *lx, struct token *after, const struct token *src)
{
	struct token *tk;

	tk = token_alloc(src);
	TAILQ_INSERT_AFTER(&lx->lx_tokens, after, tk, tk_entry);
	return tk;
}

struct token *
lexer_insert_before(struct lexer *UNUSED(lx), struct token *before, int type,
    const char *str)
{
	const struct token cp = {
		.tk_type	= type,
		.tk_lno		= before->tk_lno,
		.tk_cno		= before->tk_cno,
		.tk_str		= str,
		.tk_len		= strlen(str),
	};
	struct token *tk;

	tk = token_alloc(&cp);
	TAILQ_INSERT_BEFORE(before, tk, tk_entry);
	return tk;
}

struct token *
lexer_insert_after(struct lexer *lx, struct token *after, int type,
    const char *str)
{
	const struct token cp = {
		.tk_type	= type,
		.tk_lno		= after->tk_lno,
		.tk_cno		= after->tk_cno,
		.tk_str		= str,
		.tk_len		= strlen(str),
	};
	struct token *tk;

	tk = token_alloc(&cp);
	TAILQ_INSERT_AFTER(&lx->lx_tokens, after, tk, tk_entry);
	return tk;
}

struct token *
lexer_move_after(struct lexer *lx, struct token *after, struct token *tk)
{
	TAILQ_REMOVE(&lx->lx_tokens, tk, tk_entry);
	tk->tk_lno = after->tk_lno;
	tk->tk_cno = after->tk_cno;
	TAILQ_INSERT_AFTER(&lx->lx_tokens, after, tk, tk_entry);
	return tk;
}

struct token *
lexer_move_before(struct lexer *lx, struct token *before, struct token *tk)
{
	TAILQ_REMOVE(&lx->lx_tokens, tk, tk_entry);
	tk->tk_lno = before->tk_lno;
	tk->tk_cno = before->tk_cno;
	TAILQ_INSERT_BEFORE(before, tk, tk_entry);
	return tk;
}

void
lexer_remove(struct lexer *lx, struct token *tk, int keepfixes)
{
	assert(tk->tk_type != LEXER_EOF);

	if (keepfixes) {
		struct token *nx, *pv;

		/*
		 * Next token must always be present, we're in trouble while
		 * trying to remove the EOF token which is the only one lacking
		 * a next token.
		 */
		nx = token_next(tk);
		assert(nx != NULL);
		token_move_prefixes(tk, nx);

		pv = token_prev(tk);
		if (pv == NULL)
			pv = nx;
		token_move_suffixes(tk, pv);
	} else {
		struct token *fix;

		TAILQ_FOREACH(fix, &tk->tk_prefixes, tk_entry)
			token_branch_unlink(fix);
	}

	if (tk->tk_flags & TOKEN_FLAG_STAMP) {
		size_t i;

		tk->tk_flags &= ~TOKEN_FLAG_STAMP;
		token_rele(tk);
		for (i = 0; i < VECTOR_LENGTH(lx->lx_stamps); i++) {
			if (lx->lx_stamps[i] == tk)
				break;
		}
		for (i++; i < VECTOR_LENGTH(lx->lx_stamps); i++)
			lx->lx_stamps[i - 1] = lx->lx_stamps[i];
		VECTOR_POP(lx->lx_stamps);
	}

	if (tk->tk_flags & TOKEN_FLAG_UNMUTE) {
		assert(tk == lx->lx_unmute);
		tk->tk_flags &= ~TOKEN_FLAG_UNMUTE;
		token_rele(tk);
		lx->lx_unmute = NULL;
	}

	token_remove(&lx->lx_tokens, tk);
}

int
lexer_expect0(struct lexer *lx, int type, struct token **tk,
    const char *fun, int lno)
{
	struct token *t = NULL;

	if (!lexer_if(lx, type, &t)) {
		/*
		 * Since lexer_if() will not give us a token back in case of
		 * failure, try to peek at the next one to provide meaningful
		 * errors.
		 */
		lexer_peek(lx, &t);
		lexer_emit_error(lx, type, t, fun, lno);
		return 0;
	}
	if (tk != NULL)
		*tk = t;
	return 1;
}

void
lexer_peek_enter(struct lexer *lx, struct lexer_state *st)
{
	*st = lx->lx_st;
	lx->lx_peek++;
}

void
lexer_peek_leave(struct lexer *lx, const struct lexer_state *st)
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

/*
 * Returns non-zero if the next token(s) denotes a type.
 */
int
lexer_peek_if_type(struct lexer *lx, struct token **tk, unsigned int flags)
{
	struct lexer_state s;
	struct token *beg, *t;
	int peek = 0;
	int nkeywords = 0;
	int ntokens = 0;
	int unknown = 0;

	if (!lexer_peek(lx, &beg))
		return 0;

	lexer_peek_enter(lx, &s);
	for (;;) {
		if (lexer_peek_if(lx, LEXER_EOF, NULL))
			break;

		if (lexer_if_flags(lx,
		    TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE, &t)) {
			nkeywords++;
			peek = 1;
		} else if (lexer_if_flags(lx, TOKEN_FLAG_TYPE, &t)) {
			if (t->tk_type == TOKEN_ENUM ||
			    t->tk_type == TOKEN_STRUCT ||
			    t->tk_type == TOKEN_UNION)
				lexer_if(lx, TOKEN_IDENT, &t);
			/* Recognize constructs like `struct s[]'. */
			(void)lexer_if_pair(lx, TOKEN_LSQUARE, TOKEN_RSQUARE,
			    &t);
			peek = 1;
		} else if (lexer_if(lx, TOKEN_STAR, &t)) {
			/*
			 * A pointer is expected to only be followed by another
			 * pointer or a known type. Otherwise, the following
			 * identifier cannot be part of the type.
			 */
			if (lexer_peek_if(lx, TOKEN_IDENT, NULL))
				break;
			/* A type cannot start with a pointer. */
			if (ntokens == 0)
				break;
			peek = 1;
		} else if (lexer_peek_if_type_cpp(lx)) {
			lexer_if(lx, TOKEN_IDENT, NULL);
			lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &t);
		} else if (lexer_peek_if(lx, TOKEN_IDENT, NULL)) {
			struct lexer_state ss;
			int ident;

			/*
			 * Recognize function arguments consisting of a single
			 * type and no variable name.
			 */
			ident = 0;
			lexer_peek_enter(lx, &ss);
			if ((flags & (LEXER_TYPE_CAST | LEXER_TYPE_ARG)) &&
			    ntokens == 0 && lexer_if(lx, TOKEN_IDENT, NULL) &&
			    (lexer_if(lx, TOKEN_RPAREN, NULL) ||
			     lexer_if(lx, TOKEN_COMMA, NULL)))
				ident = 1;
			lexer_peek_leave(lx, &ss);
			if (ident) {
				if (lexer_pop(lx, &t))
					peek = 1;
				break;
			}

			/* Ensure this is not the identifier after the type. */
			if ((flags & LEXER_TYPE_CAST) == 0 &&
			    lexer_peek_if_type_ident(lx))
				break;

			/* Identifier is part of the type, consume it. */
			lexer_if(lx, TOKEN_IDENT, &t);
		} else if (ntokens > 0 && lexer_peek_if_func_ptr(lx, &t)) {
			struct token *align;

			/*
			 * Instruct parser_exec_type() where to perform ruler
			 * alignment.
			 */
			if (lexer_back(lx, &align))
				t->tk_token = align;
			peek = 1;
			break;
		} else if (ntokens > 0 && lexer_peek_if_ptr(lx, &t)) {
			peek = 1;
			break;
		} else {
			unknown = 1;
			break;
		}

		ntokens++;
	}
	lexer_peek_leave(lx, &s);

	if (ntokens > 0 && ntokens == nkeywords &&
	    (flags & LEXER_TYPE_ARG) == 0) {
		/* Only qualifier or storage token(s) cannot denote a type. */
		peek = 0;
	} else if (!peek && !unknown && ntokens > 0) {
		/*
		 * Nothing was found. However this is a sequence of identifiers
		 * (i.e. unknown types) therefore treat it as a type.
		 */
		peek = 1;
	}

	if (peek && tk != NULL)
		*tk = t;
	return peek;
}

int
lexer_if_type(struct lexer *lx, struct token **tk, unsigned int flags)
{
	struct token *t;

	if (!lexer_peek_if_type(lx, &t, flags))
		return 0;
	lx->lx_st.st_tk = t;
	if (tk != NULL)
		*tk = t;
	return 1;
}

/*
 * Peek at the next token without consuming it only if it matches the given
 * type. Returns non-zero if such token was found.
 */
int
lexer_peek_if(struct lexer *lx, int type, struct token **tk)
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
lexer_if(struct lexer *lx, int type, struct token **tk)
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
	struct token *t;

	if (!lexer_peek(lx, &t) || (t->tk_flags & flags) == 0)
		return 0;
	if (tk != NULL)
		*tk = t;
	return 1;
}

/*
 * Consume the next token if it matches the given flags. Returns non-zero such
 * token was found.
 */
int
lexer_if_flags(struct lexer *lx, unsigned int flags, struct token **tk)
{
	struct token *t;

	if (!lexer_peek_if_flags(lx, flags, &t) || !lexer_pop(lx, &t))
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
lexer_peek_if_pair(struct lexer *lx, int lhs, int rhs, struct token **tk)
{
	struct lexer_state s;
	struct token *t = NULL;
	int pair = 0;

	if (!lexer_peek_if(lx, lhs, NULL))
		return 0;

	lexer_peek_enter(lx, &s);
	for (;;) {
		if (!lexer_pop(lx, &t))
			break;
		if (t->tk_type == LEXER_EOF)
			break;
		if (t->tk_type == lhs)
			pair++;
		if (t->tk_type == rhs)
			pair--;
		if (pair == 0)
			break;
	}
	lexer_peek_leave(lx, &s);
	if (pair > 0)
		return 0;
	if (tk != NULL)
		*tk = t;
	return 1;
}

/*
 * Consume the next balanced pair of tokens such as parenthesis or squares.
 * Returns non-zero if such tokens was found.
 */
int
lexer_if_pair(struct lexer *lx, int lhs, int rhs, struct token **tk)
{
	struct token *end;

	if (!lexer_peek_if_pair(lx, lhs, rhs, &end))
		return 0;

	lx->lx_st.st_tk = end;
	if (tk != NULL)
		*tk = end;
	return 1;
}

/*
 * Peek at the prefixes of the next token without consuming it. Returns non-zero
 * if any prefix has the given flags.
 */
int
lexer_peek_if_prefix_flags(struct lexer *lx, unsigned int flags,
    struct token **tk)
{
	struct token *px, *t;

	/* Cannot use lexer_peek() as it would move past cpp branches. */
	if (!lexer_back(lx, &t))
		return 0;
	t = token_next(t);
	if (t == NULL)
		return 0;
	TAILQ_FOREACH(px, &t->tk_prefixes, tk_entry) {
		if (px->tk_flags & flags) {
			if (tk != NULL)
				*tk = px;
			return 1;
		}
	}
	return 0;
}

/*
 * Peek until the given token type is encountered. Returns non-zero if such
 * token was found.
 */
int
lexer_peek_until(struct lexer *lx, int type, struct token **tk)
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
 *
 * Assuming tk is not NULL and the stop is reached, tk will point to the stop
 * token.
 */
int
lexer_peek_until_loose(struct lexer *lx, int type, const struct token *stop,
    struct token **tk)
{
	struct lexer_state s;
	struct token *t = NULL;
	int nest = 0;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	for (;;) {
		if (!lexer_pop(lx, &t) || t == stop || t->tk_type == LEXER_EOF)
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
	if (tk != NULL)
		*tk = peek ? t : (struct token *)stop;
	return peek;
}

/*
 * Consume token(s) until the given token type is encountered. Returns non-zero
 * if such token is found.
 */
int
lexer_until(struct lexer *lx, int type, struct token **tk)
{
	for (;;) {
		struct token *t;

		if (!lexer_pop(lx, &t) || t->tk_type == LEXER_EOF)
			return 0;
		if (t->tk_type == type) {
			if (tk != NULL)
				*tk = t;
			return 1;
		}
	}
	return 0;
}

const struct diffchunk *
lexer_get_diffchunk(const struct lexer *lx, unsigned int lno)
{
	if (lx->lx_diff == NULL)
		return NULL;
	return diff_get_chunk(lx->lx_diff, lno);
}

/*
 * Looks unused but only used while debugging and therefore not declared static.
 */
void
lexer_dump(struct lexer *lx)
{
	struct token *tk;
	unsigned int i = 0;

	TAILQ_FOREACH(tk, &lx->lx_tokens, tk_entry) {
		struct token *prefix, *suffix;
		const char *str;
		int nfixes = 0;

		i++;

		TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
			str = lexer_serialize(lx, prefix);
			fprintf(stderr, "[L] %-6u   prefix %s", i, str);

			if (prefix->tk_branch.br_pv != NULL) {
				str = lexer_serialize(lx,
				    prefix->tk_branch.br_pv);
				fprintf(stderr, ", pv %s", str);
			}
			if (prefix->tk_branch.br_nx != NULL) {
				str = lexer_serialize(lx,
				    prefix->tk_branch.br_nx);
				fprintf(stderr, ", nx %s", str);
			}
			fprintf(stderr, "\n");
			nfixes++;
		}

		str = lexer_serialize(lx, tk);
		fprintf(stderr, "[L] %-6u %s\n", i, str);

		TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
			str = lexer_serialize(lx, suffix);
			fprintf(stderr, "[L] %-6u   suffix %s\n", i, str);
			nfixes++;
		}

		if (nfixes > 0)
			fprintf(stderr, "[L] -\n");
	}
}

struct token *
lexer_read(struct lexer *lx, void *UNUSED(arg))
{
	struct lexer_state st;
	struct token_list prefixes;
	struct buffer *bf;
	struct token *tk = NULL;
	struct token *t, *tmp;
	char *str;
	int ncomments = 0;
	int nlines;
	unsigned char ch;

	TAILQ_INIT(&prefixes);
	st = lx->lx_st;

	/*
	 * Consume all comments and preprocessor directives, will be hanging of
	 * the emitted token.
	 */
	for (;;) {
		struct token *last;

		if ((tmp = lexer_comment(lx, 1)) == NULL &&
		    (tmp = lexer_cpp(lx)) == NULL)
			break;

		if (tmp->tk_type == TOKEN_COMMENT &&
		    (last = TAILQ_LAST(&prefixes, token_list)) != NULL &&
		    last->tk_type == TOKEN_COMMENT &&
		    token_cmp(tmp, last) == 0)
			token_prolong(last, tmp);
		else
			TAILQ_INSERT_TAIL(&prefixes, tmp, tk_entry);
	}

	if ((tk = lexer_keyword(lx)) != NULL)
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
		tk = lexer_emit(lx, &st, delim == '"' ? &tkstr : &tklit);
		goto out;
	}

	if (isnum(ch, 1)) {
		do {
			if (lexer_getc(lx, &ch))
				goto eof;
		} while (isnum(ch, 0));
		lexer_ungetc(lx);
		tk = lexer_emit(lx, &st, &tklit);
		goto out;
	}

	if (isalpha(ch) || ch == '_') {
		while (isalnum(ch) || ch == '_') {
			if (lexer_getc(lx, &ch))
				goto eof;
		}
		lexer_ungetc(lx);

		t = lexer_find_token(lx, &st);
		if (t != NULL) {
			tk = lexer_emit(lx, &st, t);
		} else {
			/* Fallback, treat everything as an identifier. */
			tk = lexer_emit(lx, &st, &tkident);
		}
		goto out;
	}

	tk = lexer_emit(lx, &st, &tknone);
	TAILQ_CONCAT(&tk->tk_prefixes, &prefixes, tk_entry);
	str = token_sprintf(tk);
	bf = error_begin(lx->lx_er);
	buffer_printf(bf, "%s: unknown token %s\n", lx->lx_path, str);
	error_end(lx->lx_er);
	free(str);
	token_rele(tk);
	return NULL;

eof:
	tk = lexer_emit(lx, &st, &tkeof);

out:
	TAILQ_CONCAT(&tk->tk_prefixes, &prefixes, tk_entry);

	/*
	 * Consume trailing/interwined comments, will be hanging of the emitted
	 * token.
	 */
	for (;;) {
		if ((tmp = lexer_comment(lx, 0)) == NULL)
			break;
		TAILQ_INSERT_TAIL(&tk->tk_suffixes, tmp, tk_entry);
		ncomments++;
	}

	/*
	 * Trailing whitespace is only honored if it's present immediately after
	 * the token.
	 */
	if (ncomments == 0 && lexer_eat_spaces(lx, &tmp)) {
		tmp->tk_flags |= TOKEN_FLAG_OPTSPACE;
		TAILQ_INSERT_TAIL(&tk->tk_suffixes, tmp, tk_entry);
	}

	/* Consume hard lines, will be hanging of the emitted token. */
	if ((nlines = lexer_eat_lines(lx, &tmp, 0)) > 0) {
		if (nlines == 1)
			tmp->tk_flags |= TOKEN_FLAG_OPTLINE;
		TAILQ_INSERT_TAIL(&tk->tk_suffixes, tmp, tk_entry);
	}

	return tk;
}

/*
 * Consume empty line(s) until a line with optional whitespace followed by
 * something else is found.
 */
static void
lexer_eat_buffet(struct lexer *lx, struct lexer_state *oldst,
    struct lexer_state *st)
{
	int gotspaces = 0;

	*oldst = *st = lx->lx_st;

	for (;;) {
		if (lexer_eat_lines(lx, NULL, 0)) {
			*st = lx->lx_st;
			gotspaces = 0;
		} else if (gotspaces) {
			break;
		}

		if (lexer_eat_spaces(lx, NULL))
			gotspaces = 1;
		else
			break;
	}
}

static int
lexer_eat_lines(struct lexer *lx, struct token **tk, int threshold)
{
	struct lexer_state oldst, st;
	int nlines = 0;
	unsigned char ch;

	oldst = st = lx->lx_st;

	for (;;) {
		if (lexer_getc(lx, &ch))
			break;
		if (ch == '\r') {
			continue;
		} else if (ch == '\n') {
			oldst = lx->lx_st;
			if (++nlines == threshold)
				break;
		} else if (ch != ' ' && ch != '\t') {
			lexer_ungetc(lx);
			break;
		}
	}
	lx->lx_st = oldst;
	if (nlines == 0 || nlines < threshold || lexer_eof(lx))
		return 0;
	if (tk != NULL)
		*tk = lexer_emit(lx, &st, &tkline);
	return nlines;
}

static int
lexer_eat_spaces(struct lexer *lx, struct token **tk)
{
	struct lexer_state st;
	unsigned char ch;

	st = lx->lx_st;

	do {
		if (lexer_getc(lx, &ch))
			return 0;
	} while (ch == ' ' || ch == '\t');
	lexer_ungetc(lx);

	if (st.st_off == lx->lx_st.st_off)
		return 0;
	if (tk != NULL)
		*tk = lexer_emit(lx, &st, &tkspace);
	return 1;
}

static struct token *
lexer_keyword(struct lexer *lx)
{
	for (;;) {
		struct token *tk;

		while (lexer_eat_lines(lx, NULL, 0) ||
		    lexer_eat_spaces(lx, NULL))
			continue;

		tk = lexer_keyword1(lx);
		if (tk == NULL)
			break;
		if ((tk->tk_flags & TOKEN_FLAG_DISCARD) == 0)
			return tk;
	}
	return NULL;
}

static struct token *
lexer_keyword1(struct lexer *lx)
{
	struct lexer_state st = lx->lx_st;
	struct token *pv = NULL;
	struct token *tk = NULL;
	unsigned char ch;

	if (lexer_getc(lx, &ch))
		return NULL;

	for (;;) {
		struct token *ellipsis, *tmp;

		tmp = lexer_find_token(lx, &st);
		if (tmp == NULL) {
			lexer_ungetc(lx);
			tk = pv;
			break;
		}
		if ((tmp->tk_flags & TOKEN_FLAG_AMBIGUOUS) == 0) {
			tk = tmp;
			break;
		}

		/* Hack to detect ellipses since ".." is not a valid token. */
		if (tmp->tk_type == TOKEN_PERIOD &&
		    (ellipsis = lexer_ellipsis(lx, &st)) != NULL) {
			tk = ellipsis;
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
	if (tk->tk_flags & TOKEN_FLAG_DISCARD)
		return tk;
	return lexer_emit(lx, &st, tk);
}

static struct token *
lexer_comment(struct lexer *lx, int block)
{
	struct lexer_state oldst, st;
	struct token *tk;
	int cstyle;
	unsigned char ch;

	if (block) {
		lexer_eat_buffet(lx, &oldst, &st);
	} else {
		oldst = st = lx->lx_st;
		lexer_eat_spaces(lx, NULL);
	}
	if (lexer_getc(lx, &ch) || ch != '/') {
		lx->lx_st = oldst;
		return NULL;
	}
	if (lexer_getc(lx, &ch) || (ch != '/' && ch != '*')) {
		lx->lx_st = oldst;
		return NULL;
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

	if (block) {
		/*
		 * For block comments, consume trailing whitespace and up to 2
		 * hard lines(s), will be hanging of the comment token.
		 */
		lexer_eat_spaces(lx, NULL);
		lexer_eat_lines(lx, NULL, 2);
	}

	tk = lexer_emit(lx, &st, &tkcomment);
	/* Discard any remaining hard line(s). */
	if (block)
		lexer_eat_lines(lx, NULL, 0);

	return tk;
}

static struct token *
lexer_cpp(struct lexer *lx)
{
	struct lexer_state cmpst, oldst, st;
	struct token cpp;
	struct token *tk;
	int type = TOKEN_CPP;
	int comment;
	unsigned char ch;

	lexer_eat_buffet(lx, &oldst, &st);
	if (lexer_getc(lx, &ch) || ch != '#') {
		lx->lx_st = oldst;
		return NULL;
	}

	/* Space before keyword is allowed. */
	lexer_eat_spaces(lx, NULL);
	cmpst = lx->lx_st;

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
		if (ch == '/' && peek == '*') {
			comment = 1;
		} else if (comment && ch == '*' && peek == '/') {
			comment = 0;
		} else if (!comment && ch != '\\' && peek == '\n') {
			lexer_ungetc(lx);
			break;
		}
		ch = peek;
	}

	if (lexer_buffer_streq(lx, &cmpst, "if"))
		type = TOKEN_CPP_IF;
	else if (lexer_buffer_streq(lx, &cmpst, "else") ||
	    lexer_buffer_streq(lx, &cmpst, "elif"))
		type = TOKEN_CPP_ELSE;
	else if (lexer_buffer_streq(lx, &cmpst, "endif"))
		type = TOKEN_CPP_ENDIF;

	/*
	 * As cpp tokens are emitted as is, honor up to 2 hard line(s).
	 * Additional ones are excessive and will be discarded.
	 */
	lexer_eat_lines(lx, NULL, 2);

	cpp = tkcpp;
	cpp.tk_type = type;
	tk = lexer_emit(lx, &st, &cpp);
	/* Discard any remaining hard line(s). */
	lexer_eat_lines(lx, NULL, 0);
	return tk;
}

static struct token *
lexer_ellipsis(struct lexer *lx, const struct lexer_state *st)
{
	struct lexer_state oldst;
	unsigned char ch;
	int i;

	oldst = lx->lx_st;

	for (i = 0; i < 2; i++) {
		if (lexer_eof(lx) || lexer_getc(lx, &ch) || ch != '.') {
			lx->lx_st = oldst;
			return NULL;
		}
	}
	return lexer_find_token(lx, st);
}

static int
lexer_eof(const struct lexer *lx)
{
	return lx->lx_st.st_off == lx->lx_bf->bf_len;
}

static void
lexer_line_alloc(struct lexer *lx, unsigned int lno)
{
	unsigned int *dst;

	if (lx->lx_diff == NULL)
		return;

	/* We could end up here again after lexer_ungetc(). */
	if (lno - 1 < VECTOR_LENGTH(lx->lx_lines))
		return;

	dst = VECTOR_ALLOC(lx->lx_lines);
	if (dst == NULL)
		err(1, NULL);
	*dst = lx->lx_st.st_off;
}

static struct token *
lexer_find_token(const struct lexer *lx, const struct lexer_state *st)
{
	struct token_hash *th;
	const char *key;
	size_t len;

	len = lx->lx_st.st_off - st->st_off;
	key = &lx->lx_bf->bf_ptr[st->st_off];
	HASH_FIND(th_hh, tokens, key, len, th);
	if (th == NULL)
		return NULL;
	return &th->th_tk;
}

static int
lexer_buffer_streq(const struct lexer *lx, const struct lexer_state *st,
    const char *str)
{
	const char *buf;
	size_t buflen, len;

	buflen = lx->lx_st.st_off - st->st_off;
	if (buflen == 0)
		return 0;
	len = strlen(str);
	if (len > buflen)
		return 0;
	buf = &lx->lx_bf->bf_ptr[st->st_off];
	return strncmp(buf, str, len) == 0;
}

static void
lexer_emit_error(struct lexer *lx, int type, const struct token *tk,
    const char *fun, int lno)
{
	struct buffer *bf;
	struct token *t;
	const char *exp, *got;

	/* Be quiet while about to branch. */
	if (lexer_back(lx, &t) && token_is_branch(t)) {
		lexer_trace(lx, "%s:%d: suppressed, expected %s", fun, lno,
		    lexer_serialize(lx, &(struct token){.tk_type = type}));
		return;
	}

	/* Be quiet if an error already has been emitted. */
	if (lx->lx_st.st_err++ > 0)
		return;

	/* Be quiet while peeking. */
	if (lx->lx_peek > 0)
		return;

	bf = error_begin(lx->lx_er);
	buffer_printf(bf, "%s: ", lx->lx_path);
	if (trace(lx->lx_op, 'l'))
		buffer_printf(bf, "%s:%d: ", fun, lno);
	exp = lexer_serialize(lx, &(struct token){.tk_type = type});
	got = tk != NULL ? lexer_serialize(lx, tk) : NULL;
	buffer_printf(bf, "expected type %s got %s\n", exp,
	    got != NULL ? got : "(null)");
	error_end(lx->lx_er);
}

/*
 * Prolong the dst token to also cover the src token. They are required to be of
 * the same type and be adjacent to each other.
 */
static void
token_prolong(struct token *dst, struct token *src)
{
	assert(src->tk_type == dst->tk_type);
	assert(src->tk_off >= dst->tk_off + dst->tk_len);
	dst->tk_len += src->tk_len;
	token_rele(src);
}

static int
lexer_peek_if_func_ptr(struct lexer *lx, struct token **tk)
{
	struct lexer_state s;
	struct token *lparen, *rparen;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_LPAREN, NULL) &&
	    lexer_if(lx, TOKEN_STAR, NULL)) {
		struct token *ident = NULL;

		while (lexer_if(lx, TOKEN_STAR, NULL))
			continue;

		lexer_if_flags(lx, TOKEN_FLAG_QUALIFIER, NULL);
		lexer_if(lx, TOKEN_IDENT, &ident);
		if (lexer_if(lx, TOKEN_LSQUARE, NULL)) {
			lexer_if(lx, TOKEN_LITERAL, NULL);
			lexer_if(lx, TOKEN_RSQUARE, NULL);
		}
		if (lexer_if(lx, TOKEN_RPAREN, &rparen)) {
			if (lexer_peek_if(lx, TOKEN_LPAREN, &lparen) &&
			    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, tk)) {
				/*
				 * Annotate the left parenthesis, used by
				 * parser_exec_type().
				 */
				lparen->tk_flags |= TOKEN_FLAG_TYPE_ARGS;

				peek = 1;
			} else if (ident == NULL &&
			    (lexer_if(lx, TOKEN_RPAREN, NULL) ||
			     lexer_if(lx, LEXER_EOF, NULL))) {
				/*
				 * Function pointer lacking arguments wrapped in
				 * parenthesis. Be careful here in order to not
				 * confuse a function call.
				 */
				peek = 1;
				*tk = rparen;
			}
		}
	}
	lexer_peek_leave(lx, &s);

	return peek;
}

static int
lexer_peek_if_ptr(struct lexer *lx, struct token **UNUSED(tk))
{
	struct lexer_state s;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_LPAREN, NULL) &&
	    lexer_if(lx, TOKEN_STAR, NULL) &&
	    lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if(lx, TOKEN_RPAREN, NULL) &&
	    lexer_if(lx, TOKEN_LSQUARE, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);
	return peek;
}

static int
lexer_peek_if_type_ident(struct lexer *lx)
{
	struct lexer_state s;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, NULL) &&
	    (lexer_if_flags(lx, TOKEN_FLAG_ASSIGN, NULL) ||
	     lexer_if(lx, TOKEN_LSQUARE, NULL) ||
	     (lexer_if(lx, TOKEN_LPAREN, NULL) &&
	      !lexer_peek_if(lx, TOKEN_STAR, NULL)) ||
	     lexer_if(lx, TOKEN_RPAREN, NULL) ||
	     lexer_if(lx, TOKEN_SEMI, NULL) ||
	     lexer_if(lx, TOKEN_COMMA, NULL) ||
	     lexer_if(lx, TOKEN_COLON, NULL) ||
	     lexer_if(lx, TOKEN_ATTRIBUTE, NULL)))
		peek = 1;
	lexer_peek_leave(lx, &s);

	return peek;
}

/*
 * Detect usage of types hidden behind cpp such as STACK_OF(X509).
 */
static int
lexer_peek_if_type_cpp(struct lexer *lx)
{
	struct lexer_state s;
	struct token *ident;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if(lx, TOKEN_LPAREN, NULL) &&
	    lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if(lx, TOKEN_RPAREN, NULL)) {
		struct token *nx;

		if (lexer_if(lx, TOKEN_IDENT, &nx)) {
			size_t len = nx->tk_len;

			/* Ugly, do not confuse cppx. */
			if (!lexer_peek_if(lx, TOKEN_LPAREN, NULL) &&
			    (ident->tk_len != len ||
			     strncmp(ident->tk_str, nx->tk_str, len) != 0))
				peek = 1;
		} else if (lexer_if(lx, TOKEN_STAR, NULL)) {
			peek = 1;
		}
	}
	lexer_peek_leave(lx, &s);
	return peek;
}

/*
 * Fold tokens covered by the branch into a prefix.
 */
static void
lexer_branch_fold(struct lexer *lx, struct token *src)
{
	struct token *dst, *prefix, *pv, *rm;
	size_t len, off;
	int unmute = 0;

	/* Grab a reference since the branch is about to be removed. */
	dst = src->tk_branch.br_nx;
	token_ref(dst);

	off = src->tk_off;
	len = (dst->tk_off + dst->tk_len) - off;

	prefix = token_alloc(&tkcpp);
	prefix->tk_lno = src->tk_lno;
	prefix->tk_cno = src->tk_cno;
	prefix->tk_off = off;
	prefix->tk_str = &lx->lx_bf->bf_ptr[off];
	prefix->tk_len = len;

	/*
	 * Remove all prefixes hanging of the destination covered by the new
	 * prefix token.
	 */
	while (!TAILQ_EMPTY(&dst->tk_token->tk_prefixes)) {
		struct token *pr;

		pr = TAILQ_FIRST(&dst->tk_token->tk_prefixes);
		lexer_trace(lx, "removing prefix %s", lexer_serialize(lx, pr));
		TAILQ_REMOVE(&dst->tk_token->tk_prefixes, pr, tk_entry);
		/* Completely unlink any branch. */
		while (token_branch_unlink(pr) == 0)
			continue;
		token_rele(pr);
		if (pr == dst)
			break;
	}

	lexer_trace(lx, "add prefix %s to %s",
	    lexer_serialize(lx, prefix),
	    lexer_serialize(lx, dst->tk_token));
	TAILQ_INSERT_HEAD(&dst->tk_token->tk_prefixes, prefix, tk_entry);

	/*
	 * Keep any existing prefix not covered by the new prefix token
	 * by moving them to the destination.
	 */
	pv = token_prev(src);
	for (;;) {
		struct token *tmp;

		if (pv == NULL)
			break;

		lexer_trace(lx, "keeping prefix %s", lexer_serialize(lx, pv));
		tmp = token_prev(pv);
		token_move_prefix(pv, src->tk_token, dst->tk_token);
		pv = tmp;
	}

	/*
	 * Remove all tokens up to the destination covered by the new prefix
	 * token.
	 */
	rm = src->tk_token;
	for (;;) {
		struct token *nx;

		if (rm == dst->tk_token)
			break;

		nx = token_next(rm);
		lexer_trace(lx, "removing %s", lexer_serialize(lx, rm));
		if (rm->tk_flags & TOKEN_FLAG_UNMUTE)
			unmute = 1;
		lexer_remove(lx, rm, 0);
		rm = nx;
	}

	/*
	 * If the unmute token ended up being deleted, tell doc_token() that
	 * crossing the end of this branch must cause tokens to be emitted
	 * again.
	 */
	if (unmute)
		lexer_branch_unmute(lx, dst->tk_token);

	token_rele(dst);
}

static void
lexer_branch_unmute(struct lexer *lx, struct token *tk)
{
	assert(lx->lx_unmute == NULL);
	tk->tk_flags |= TOKEN_FLAG_UNMUTE;
	token_ref(tk);
	lx->lx_unmute = tk;
}

/*
 * Find the best suited branch to fold relative to the given token while trying
 * to recover after encountering invalid source code. We do not want to fold a
 * partially consumed branch as lexer_branch() already has removed tokens making
 * it impossible to traverse the same source code again since it is no longer
 * intact. However, when reaching EOF we try to fold even partially consumed
 * branches.
 */
static struct token *
lexer_recover_branch(struct token *tk)
{
	unsigned int flags[] = {
		LEXER_BRANCH_BACKWARD | LEXER_BRANCH_INTACT,
		LEXER_BRANCH_FORWARD | LEXER_BRANCH_INTACT,
		LEXER_BRANCH_BACKWARD,
		LEXER_BRANCH_FORWARD,
	};
	size_t nflags = sizeof(flags) / sizeof(flags[0]);
	size_t i;

	for (i = 0; i < nflags; i++) {
		struct token *br;

		br = lexer_recover_branch1(tk, flags[i]);
		if (br != NULL)
			return br;
	}
	return NULL;
}

static struct token *
lexer_recover_branch1(struct token *tk, unsigned int flags)
{
	for (;;) {
		struct token *prefix;

		TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
			if (prefix->tk_type == TOKEN_CPP_IF)
				return prefix;
			if (prefix->tk_type == TOKEN_CPP_ENDIF) {
				struct token *pv = prefix->tk_branch.br_pv;

				if ((flags & LEXER_BRANCH_INTACT) &&
				    pv->tk_type == TOKEN_CPP_ELSE &&
				    pv->tk_branch.br_pv == NULL)
					return NULL;
				return pv;
			}
		}

		if (flags & LEXER_BRANCH_FORWARD)
			tk = token_next(tk);
		else if (flags & LEXER_BRANCH_BACKWARD)
			tk = token_prev(tk);
		else
			tk = NULL;
		if (tk == NULL)
			break;
	}

	return NULL;
}

static void
lexer_trace0(const struct lexer *UNUSED(lx), const char *fun, const char *fmt,
    ...)
{
	va_list ap;

	fprintf(stderr, "[L] %s: ", fun);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

static int
isnum(unsigned char ch, int prefix)
{
	if (prefix)
		return isdigit(ch);

	ch = tolower(ch);
	return isdigit(ch) || isxdigit(ch) || ch == 'l' || ch == 'x' ||
	    ch == 'u' || ch == '.';
}
