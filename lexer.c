#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UTHASH
#  include <uthash.h>
#else
#  include "compat-uthash.h"
#endif

#include "extern.h"

struct branch {
	struct token		*br_cpp;
	TAILQ_ENTRY(branch)	 br_entry;
};

TAILQ_HEAD(branch_list, branch);

struct lexer {
	struct lexer_state	 lx_st;
	struct error		*lx_er;
	const struct config	*lx_cf;
	struct buffer		*lx_bf;
	const char		*lx_path;

	int		lx_eof;
	int		lx_peek;
	enum token_type	lx_expect;

#define NMARKERS	2
	struct token	*lx_markers[NMARKERS];

	struct token_list	lx_tokens;
	struct branch_list	lx_branches;
};

struct token_hash {
	struct token	th_tk;
	UT_hash_handle	th_hh;
};

static int		 lexer_getc(struct lexer *, unsigned char *);
static void		 lexer_ungetc(struct lexer *);
static int		 lexer_read(struct lexer *, struct token **);
static int		 lexer_eat_lines(struct lexer *, struct token **, int);
static struct token	*lexer_eat_space(struct lexer *, int, int);
static struct token	*lexer_keyword(struct lexer *);
static struct token	*lexer_comment(struct lexer *, int);
static struct token	*lexer_cpp(struct lexer *);
static struct token	*lexer_ellipsis(struct lexer *,
    const struct lexer_state *);
static int		 lexer_eof(const struct lexer *);

static int	lexer_find_token(const struct lexer *,
    const struct lexer_state *, struct token **);
static int	lexer_buffer_strcmp(const struct lexer *,
    const struct lexer_state *, const char *);

static struct token	*lexer_emit(struct lexer *, const struct lexer_state *,
    const struct token *);
static struct token	*lexer_emit_fake(struct lexer *, enum token_type,
    struct token *);
static void		 lexer_emit_error(struct lexer *, enum token_type,
    const struct token *, const char *, int);

static int	lexer_peek_if_func_ptr(struct lexer *, struct token **);

static struct token	*lexer_recover_fold(struct lexer *, struct token *,
    struct token *, const struct token *);
static int		 lexer_recover_hard(struct lexer *, struct token *);
static void		 lexer_recover_reset(struct lexer *, struct token *);

static struct token	*lexer_branch_find(struct token *);
static struct token	*lexer_branch_next(const struct lexer *);
static void		 lexer_branch_enter(struct lexer *, struct token *,
    struct token *);
static void		 lexer_branch_leave(struct lexer *, struct token *,
    struct token *);
static void		 lexer_branch_link(struct lexer *, struct token *,
    struct token *);

#define lexer_trace(lx, fmt, ...) do {					\
	if (UNLIKELY((lx)->lx_cf->cf_verbose >= 2))			\
		__lexer_trace((lx), __func__, (fmt),			\
		    __VA_ARGS__);					\
} while (0)
static void	__lexer_trace(const struct lexer *, const char *, const char *,
    ...)
	__attribute__((__format__(printf, 3, 4)));

static int	isnum(unsigned char, int);

static int		 token_branch_cover(const struct token *,
    const struct token *);
static void		 token_branch_link(struct token *, struct token *);
static void		 token_branch_unlink(struct token *);
static struct token	*token_get_branch(struct token *);
static struct token	*token_find_prefix(const struct token *,
    enum token_type);
static void		 token_free(struct token *);
static void		 token_list_free(struct token_list *);
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
static const struct token	tkunknown = {
	.tk_type	= TOKEN_UNKNOWN,
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
token_has_dangling(const struct token *tk)
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

/*
 * Returns non-zero if given token has a branch continuation associated with it.
 */
int
token_is_branch(const struct token *tk)
{
	return token_get_branch((struct token *)tk) != NULL;
}

/*
 * Returns non-zero if the given token represents a declaration of the given
 * type.
 */
int
token_is_decl(const struct token *tk, enum token_type type)
{
	const struct token *nx;

	nx = TAILQ_NEXT(tk, tk_entry);
	if (nx == NULL || nx->tk_type != TOKEN_LBRACE)
		return 0;

	if (tk->tk_type == TOKEN_IDENT)
		tk = TAILQ_PREV(tk, token_list, tk_entry);
	if (tk == NULL)
		return 0;
	return tk->tk_type == type;
}

/*
 * Remove any space suffixes from the given token.
 */
void
token_trim(struct token *tk)
{
	struct token *suffix, *tmp;

	TAILQ_FOREACH_SAFE(suffix, &tk->tk_suffixes, tk_entry, tmp) {
		if (suffix->tk_type == TOKEN_SPACE) {
			TAILQ_REMOVE(&tk->tk_suffixes, suffix, tk_entry);
			token_free(suffix);
		}
	}
}

char *
token_sprintf(const struct token *tk)
{
	char *buf = NULL;
	char *val;
	const char *type;
	ssize_t bufsiz = 0;
	int i;

	type = strtoken(tk->tk_type);
	val = strnice(tk->tk_str, tk->tk_len);
	for (i = 0; i < 2; i++) {
		int n;

		n = snprintf(buf, bufsiz, "%s<%u:%u>(\"%s\")", type,
		    tk->tk_lno, tk->tk_cno, val);
		if (n < 0 || (buf != NULL && n >= bufsiz))
			errc(1, ENAMETOOLONG, "snprintf");
		if (buf == NULL) {
			bufsiz = n + 1;
			buf = malloc(bufsiz);
			if (buf == NULL)
				err(1, NULL);
		}
	}
	free(val);
	return buf;
}

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
#define A(t, s, f)	T(t, s, f)
#include "token.h"
	};
	unsigned int i;

	for (i = 0; keywords[i].th_tk.tk_type != TOKEN_NONE; i++) {
		struct token_hash *th = &keywords[i];

		if (th->th_tk.tk_len > 0)
			HASH_ADD_KEYPTR(th_hh, tokens, th->th_tk.tk_str,
			    th->th_tk.tk_len, th);
	}
}

void
lexer_shutdown(void)
{
	HASH_CLEAR(th_hh, tokens);
}

struct lexer *
lexer_alloc(const char *path, struct error *er, const struct config *cf)
{
	struct branch *br;
	struct buffer *bf;
	struct lexer *lx;
	int error = 0;

	bf = buffer_read(path);
	if (bf == NULL)
		return NULL;

	lx = calloc(1, sizeof(*lx));
	if (lx == NULL)
		err(1, NULL);
	lx->lx_er = er;
	lx->lx_cf = cf;
	lx->lx_bf = bf;
	lx->lx_path = path;
	lx->lx_expect = TOKEN_NONE;
	lx->lx_st.st_lno = 1;
	lx->lx_st.st_cno = 1;
	TAILQ_INIT(&lx->lx_tokens);
	TAILQ_INIT(&lx->lx_branches);

	for (;;) {
		struct token *tk;

		if (!lexer_read(lx, &tk)) {
			error = 1;
			break;
		}
		if (tk->tk_type == TOKEN_EOF)
			break;
	}

	/* Remove any pending broken branches. */
	while ((br = TAILQ_FIRST(&lx->lx_branches)) != NULL) {
		TAILQ_REMOVE(&lx->lx_branches, br, br_entry);
		free(br);
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

const struct buffer *
lexer_get_buffer(struct lexer *lx)
{
	buffer_appendc(lx->lx_bf, '\0');
	return lx->lx_bf;
}

int
lexer_get_error(const struct lexer *lx)
{
	return lx->lx_st.st_err;
}

void
lexer_recover_mark(struct lexer *lx)
{
	int i = 0;

	/* Remove the first entry by shifting everything to the left. */
	for (i = 0; i < NMARKERS - 1; i++) {
		if (lx->lx_markers[i + 1] == NULL)
			break;
		lx->lx_markers[i] = lx->lx_markers[i + 1];
		lx->lx_markers[i + 1] = NULL;
	}

	/* Find the first empty slot. */
	for (i = 0; i < NMARKERS - 1; i++) {
		if (lx->lx_markers[i] == NULL)
			break;
	}

	if (!lexer_peek(lx, &lx->lx_markers[i]))
		lx->lx_markers[i] = NULL;
}

/*
 * Try to recover after encountering invalid code.
 */
int
lexer_recover(struct lexer *lx)
{
	struct token *back, *br, *dst, *src, *start, *tk;
	int nmarkers = 0;
	int m;

	if (!lexer_back(lx, &back) && !lexer_peek(lx, &back))
		return 0;

	for (m = 0; m < NMARKERS; m++) {
		if (lx->lx_markers[m] == NULL)
			break;

		lexer_trace(lx, "marker %s", token_sprintf(lx->lx_markers[m]));
		nmarkers++;
	}

	/* Start from the last marked token. */
	for (m = NMARKERS - 1; m >= 0; m--) {
		start = lx->lx_markers[m];
		if (start != NULL)
			break;
	}
	if (start == NULL)
		return 0;
	tk = start;

	/*
	 * Find the first branch by looking forward from the start token. Note,
	 * we could be inside a branch.
	 */
	lexer_trace(lx, "start %s, back %s", token_sprintf(tk),
	    token_sprintf(back));
	br = lexer_branch_find(tk);
	if (br == NULL)
		return lexer_recover_hard(lx, start);

	src = br->tk_token;
	dst = br->tk_branch.br_nx->tk_token;
	lexer_trace(lx, "branch from %s to %s covering [%s, %s)",
	    token_sprintf(br), token_sprintf(br->tk_branch.br_nx),
	    token_sprintf(src), token_sprintf(dst));

	if (token_branch_cover(br, start)) {
		lexer_trace(lx, "start %s covered by branch [%s, %s]",
		    token_sprintf(start), token_sprintf(br),
		    token_sprintf(br->tk_branch.br_nx));

		/*
		 * Move forward the start as the same token is about to be
		 * removed.
		 */
		start = dst;
	} else if (lexer_recover_hard(lx, start)) {
		/*
		 * Since the branch does not cover the start token, we might be
		 * better of doing a hard recover.
		 */
		return 1;
	} else {
		/* Find the first marked token positioned before the branch. */
		for (;;) {
			start = lx->lx_markers[m];
			if (token_cmp(start, br) < 0)
				break;

			if (m == 0)
				break;
			m--;
			lexer_trace(lx,
			    "start %s before branch, moving backwards...",
			    token_sprintf(start));
		}
	}

	/* Turn the whole branch into a prefix hanging of the destination. */
	lexer_recover_fold(lx, src, dst, br->tk_branch.br_nx);

	lexer_recover_reset(lx, start);
	lexer_trace(lx, "removing %d document(s)", nmarkers - m);
	return nmarkers - m;
}

/*
 * Returns non-zero if the lexer took the next branch.
 */
int
__lexer_branch(struct lexer *lx, struct token **tk, const char *fun, int lno)
{
	struct token *br, *dst, *rm, *seek;

	br = lexer_branch_next(lx);
	if (br == NULL)
		return 0;

	dst = br->tk_branch.br_nx->tk_token;
	seek = tk != NULL ? *tk : dst;

	lexer_trace(lx, "from %s:%d", fun, lno);
	lexer_trace(lx, "branch from %s to %s, covering [%s, %s)",
	    token_sprintf(br), token_sprintf(br->tk_branch.br_nx),
	    token_sprintf(br->tk_token),
	    token_sprintf(br->tk_branch.br_nx->tk_token));

	rm = br->tk_token;

	for (;;) {
		struct token *nx;

		lexer_trace(lx, "removing %s", token_sprintf(rm));

		/*
		 * Move the seek token forward if we stamped a token about to be
		 * removed.
		 */
		if (rm == seek)
			seek = dst;

		nx = TAILQ_NEXT(rm, tk_entry);
		TAILQ_REMOVE(&lx->lx_tokens, rm, tk_entry);
		token_free(rm);
		if (nx == dst)
			break;
		rm = nx;
	}

	/*
	 * Tell doc_token() that crossing this token must cause tokens to be
	 * emitted again.
	 */
	dst->tk_flags |= TOKEN_FLAG_UNMUTE;

	/* Rewind causing the seek token to be next one to emit. */
	lexer_trace(lx, "seek to %s", token_sprintf(seek));
	lx->lx_st.st_tok = TAILQ_PREV(seek, token_list, tk_entry);
	lx->lx_st.st_err = 0;
	if (tk != NULL)
		*tk = seek;
	return 1;
}

/*
 * Returns the next branch continuation token if present.
 */
int
lexer_is_branch(const struct lexer *lx)
{
	return lexer_branch_next(lx) != NULL;
}

/*
 * Returns non-zero if the next token denotes the end of a branch.
 */
int
lexer_is_branch_end(const struct lexer *lx)
{
	struct token *tk;

	/* Cannot use lexer_peek() as it would move past the branch. */
	if (!lexer_back(lx, &tk))
		return 0;
	tk = TAILQ_NEXT(tk, tk_entry);
	return tk != NULL && token_find_prefix(tk, TOKEN_CPP_ENDIF);
}

int
lexer_pop(struct lexer *lx, struct token **tk)
{
	struct lexer_state *st = &lx->lx_st;

	if (TAILQ_EMPTY(&lx->lx_tokens))
		return 0;

	if (st->st_tok == NULL) {
		st->st_tok = TAILQ_FIRST(&lx->lx_tokens);
	} else if (st->st_tok->tk_type != TOKEN_EOF) {
		/* Do not move passed a branch. */
		if (lx->lx_peek == 0 && token_is_branch(st->st_tok))
			return 0;

		st->st_tok = TAILQ_NEXT(st->st_tok, tk_entry);
		if (!token_is_branch(st->st_tok))
			goto out;

		if (lx->lx_peek == 0) {
			/*
			 * While not peeking, instruct the parser to halt.
			 * Calling lexer_branch() allows the parser to continue
			 * execution by taking the next branch.
			 */
			lexer_trace(lx, "halt at %s",
			    token_sprintf(st->st_tok));
			return 0;
		} else {
			struct token *br = st->st_tok;

			/* While peeking, act as taking the current branch. */
			while (br->tk_branch.br_nx != NULL)
				br = br->tk_branch.br_nx;
			st->st_tok = br;
		}
	}

out:
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
	struct token *t = NULL;
	struct token *pv;

	if (!lexer_back(lx, &pv))
		pv = NULL;

	if (!lexer_pop(lx, &t))
		goto err;
	if (t->tk_type == type) {
		if (tk != NULL)
			*tk = t;
		return 1;
	}

err:
	if (lx->lx_expect == TOKEN_NONE)
		lx->lx_expect = type;
	lx->lx_st.st_tok = pv;
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

/*
 * Returns non-zero if the next token(s) denotes a type.
 */
int
lexer_peek_if_type(struct lexer *lx, struct token **tk)
{
	struct lexer_state s;
	struct token *beg, *t;
	int peek = 0;
	int ntokens = 0;
	int unknown = 0;

	if (!lexer_peek(lx, &beg))
		return 0;

	lexer_peek_enter(lx, &s);
	for (;;) {
		unsigned int flags = TOKEN_FLAG_TYPE | TOKEN_FLAG_QUALIFIER |
		    TOKEN_FLAG_STORAGE;

		if (lexer_peek_if(lx, TOKEN_EOF, NULL))
			break;

		if (lexer_if_flags(lx, flags, &t)) {
			if (t->tk_flags & TOKEN_FLAG_IDENT)
				lexer_if(lx, TOKEN_IDENT, &t);
			/* Recognize constructs like `struct s[]' for instance. */
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

			/*
			 * Ensure this is not an identifier which is not part of
			 * the type.
			 */
			ident = 1;
			lexer_peek_enter(lx, &ss);
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
				ident = 0;
			lexer_peek_leave(lx, &ss);
			if (!ident)
				break;

			/* Consume the identifier, i.e. preprocessor macro. */
			lexer_if(lx, TOKEN_IDENT, &t);
		} else if (lexer_peek_if_func_ptr(lx, &t)) {
			struct token *align;

			/*
			 * Instruct parser_exec_type() where to perform ruler
			 * alignment.
			 */
			if (lexer_back(lx, &align))
				t->tk_token = align;
			peek = 1;
			break;
		} else {
			unknown = 1;
			break;
		}

		ntokens++;
	}
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

int
lexer_if_type(struct lexer *lx, struct token **tk)
{
	struct token *t;

	if (!lexer_peek_if_type(lx, &t))
		return 0;
	lx->lx_st.st_tok = t;
	if (tk != NULL)
		*tk = t;
	return 1;
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
	struct token *t = NULL;
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

	if (lx->lx_st.st_err == 0 && pair == 0) {
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
		struct token *t = NULL;

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

/*
 * Looks unused but only used while debugging and therefore not declared static.
 */
void
lexer_dump(const struct lexer *lx)
{
	struct token *tk;

	TAILQ_FOREACH(tk, &lx->lx_tokens, tk_entry) {
		struct token *prefix, *suffix;

		TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
			fprintf(stderr, "  prefix %s", token_sprintf(prefix));
			if (prefix->tk_branch.br_pv != NULL)
				fprintf(stderr, ", pv %s",
				    token_sprintf(prefix->tk_branch.br_pv));
			if (prefix->tk_branch.br_nx != NULL)
				fprintf(stderr, ", nx %s",
				    token_sprintf(prefix->tk_branch.br_nx));
			fprintf(stderr, "\n");
		}
		fprintf(stderr, "%s\n", token_sprintf(tk));
		TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
			fprintf(stderr, "  suffix %s\n", token_sprintf(suffix));
		}
	}
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
	struct token *t, *tmp;
	int error = 0;
	unsigned char ch;

	TAILQ_INIT(&dangling);
	st = lx->lx_st;

	if (lx->lx_st.st_err)
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

	/* Look for keywords but ignore discarded ones. */
	for (;;) {
		lexer_eat_space(lx, 1, 0);
		if ((*tk = lexer_keyword(lx)) == NULL)
			break;
		if ((*tk)->tk_flags & TOKEN_FLAG_DISCARD)
			continue;
		goto out;
	}

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
		} else {
			/* Fallback, treat everything as an identifier. */
			*tk = lexer_emit(lx, &st, &tkident);
		}
		goto out;
	}

	*tk = lexer_emit(lx, &st, &tkunknown);
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
		/*
		 * Halt on hard line(s) since this must be a trailing comment
		 * meaning no further comment(s) can be associated with this
		 * token.
		 */
		if (tmp->tk_flags & TOKEN_FLAG_NEWLINE)
			break;
	}

	/* Consume hard lines, will be hanging of the emitted token. */
	if (lexer_eat_lines(lx, &tmp, 2))
		TAILQ_INSERT_TAIL(&(*tk)->tk_suffixes, tmp, tk_entry);

	/*
	 * Establish links between cpp branches.
	 */
	TAILQ_FOREACH(tmp, &(*tk)->tk_prefixes, tk_entry) {
		switch (tmp->tk_type) {
		case TOKEN_CPP_IF:
			lexer_branch_enter(lx, tmp, *tk);
			break;
		case TOKEN_CPP_ELSE:
			lexer_branch_link(lx, tmp, *tk);
			break;
		case TOKEN_CPP_ENDIF:
			lexer_branch_leave(lx, tmp, *tk);
			break;
		default:
			break;
		}
	}

	return error ? 0 : 1;
}

static int
lexer_eat_lines(struct lexer *lx, struct token **tk, int threshold)
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
	if (nlines < threshold || lexer_eof(lx))
		return 0;
	if (tk != NULL)
		*tk = lexer_emit(lx, &st, &tkline);
	return nlines;
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
lexer_keyword(struct lexer *lx)
{
	struct lexer_state st = lx->lx_st;
	struct token *pv = NULL;
	struct token *tk = NULL;
	unsigned char ch;

	if (lexer_getc(lx, &ch))
		return NULL;

	for (;;) {
		struct token *ellipsis, *tmp;

		if (!lexer_find_token(lx, &st, &tmp)) {
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
	int ncomments = 0;
	int nlines;

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

	if (block) {
		/*
		 * For block comments, consume trailing whitespace and hard lines(s),
		 * will be hanging of the comment token.
		 */
		(void)lexer_eat_space(lx, 0, 0);
		lexer_eat_lines(lx, NULL, 2);
		return lexer_emit(lx, &st, &tkcomment);
	}

	/*
	 * For trailing comments, take note trailing hard line which will be emitted
	 * by doc_token().
	 */
	tk = lexer_emit(lx, &st, &tkcomment);
	if ((nlines = lexer_eat_lines(lx, NULL, 1)) > 0) {
		tk->tk_flags |= TOKEN_FLAG_NEWLINE;
		tk->tk_int = nlines;
	}
	return tk;
}

static struct token *
lexer_cpp(struct lexer *lx)
{
	struct lexer_state cmpst, st;
	struct token cpp;
	enum token_type type = TOKEN_CPP;
	int comment;
	unsigned char ch;

	st = lx->lx_st;
	lexer_eat_space(lx, 1, 0);

	if (lexer_getc(lx, &ch) || ch != '#') {
		lx->lx_st = st;
		return NULL;
	}

	/* Space before keyword is allowed. */
	lexer_eat_space(lx, 0, 0);
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
		if (ch == '/' && peek == '*')
			comment = 1;
		else if (comment && ch == '*' && peek == '/')
			comment = 0;
		else if (!comment && ch != '\\' && peek == '\n')
			break;
		ch = peek;
	}

	if (lexer_buffer_strcmp(lx, &cmpst, "if") == 0)
		type = TOKEN_CPP_IF;
	else if (lexer_buffer_strcmp(lx, &cmpst, "else") == 0 ||
	    lexer_buffer_strcmp(lx, &cmpst, "elif") == 0)
		type = TOKEN_CPP_ELSE;
	else if (lexer_buffer_strcmp(lx, &cmpst, "endif") == 0)
		type = TOKEN_CPP_ENDIF;

	/* Consume hard line(s), will be hanging of the cpp token. */
	lexer_eat_lines(lx, NULL, 2);

	cpp = tkcpp;
	cpp.tk_type = type;
	return lexer_emit(lx, &st, &cpp);
}

static struct token *
lexer_ellipsis(struct lexer *lx, const struct lexer_state *st)
{
	struct lexer_state oldst;
	struct token *tk;
	unsigned char ch;
	int i;

	oldst = lx->lx_st;

	for (i = 0; i < 2; i++) {
		if (lexer_eof(lx) || lexer_getc(lx, &ch) || ch != '.') {
			lx->lx_st = oldst;
			return NULL;
		}
	}
	if (!lexer_find_token(lx, st, &tk))
		return NULL;
	return tk;
}

static int
lexer_eof(const struct lexer *lx)
{
	return lx->lx_st.st_off == lx->lx_bf->bf_len;
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
	return strncmp(buf, str, len);
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
	t->tk_off = st->st_off;
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

static struct token *
lexer_emit_fake(struct lexer *lx, enum token_type type, struct token *after)
{
	struct token *t;
	struct token_hash *th, *tmp;

	HASH_ITER(th_hh, tokens, th, tmp) {
		if (th->th_tk.tk_type == type)
			break;
	}
	assert(th != NULL);

	t = calloc(1, sizeof(*t));
	if (t == NULL)
		err(1, NULL);
	t->tk_type = type;
	t->tk_flags = TOKEN_FLAG_FAKE;
	t->tk_str = th->th_tk.tk_str;
	t->tk_len = th->th_tk.tk_len;
	TAILQ_INIT(&t->tk_prefixes);
	TAILQ_INIT(&t->tk_suffixes);
	TAILQ_INSERT_AFTER(&lx->lx_tokens, after, t, tk_entry);
	return t;
}

static void
lexer_emit_error(struct lexer *lx, enum token_type type,
    const struct token *tk, const char *fun, int lno)
{
	struct token *t;
	char *str;

	/* Be quiet while about to branch. */
	if (lexer_back(lx, &t) && token_is_branch(t)) {
		lexer_trace(lx, "%s:%d: suppressed, expected %s", fun, lno,
		    strtoken(type));
		return;
	}

	/* Be quiet if an error already has been emitted. */
	if (lx->lx_st.st_err++ > 0)
		return;

	/* Be quiet while peeking. */
	if (lx->lx_peek > 0)
		return;

	error_write(lx->lx_er, "%s: ", lx->lx_path);
	if (lx->lx_cf->cf_verbose > 0)
		error_write(lx->lx_er, "%s:%d: ", fun, lno);
	str = tk ? token_sprintf(tk) : NULL;
	error_write(lx->lx_er, "expected type %s got %s\n", strtoken(type),
	    str ? str : "(null)");
	free(str);
}

static int
lexer_peek_if_func_ptr(struct lexer *lx, struct token **tk)
{
	struct lexer_state s;
	struct token *lparen;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_LPAREN, NULL) &&
	    lexer_if(lx, TOKEN_STAR, NULL)) {
		while (lexer_if(lx, TOKEN_STAR, NULL))
			continue;

		lexer_if_flags(lx, TOKEN_FLAG_QUALIFIER, NULL);
		lexer_if(lx, TOKEN_IDENT, NULL);
		if (lexer_if(lx, TOKEN_LSQUARE, NULL)) {
			lexer_if(lx, TOKEN_LITERAL, NULL);
			lexer_if(lx, TOKEN_RSQUARE, NULL);
		}
		if (lexer_if(lx, TOKEN_RPAREN, NULL) &&
		    lexer_peek_if(lx, TOKEN_LPAREN, &lparen) &&
		    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, tk)) {
			/*
			 * Annotate the left parenthesis, used by
			 * parser_exec_type().
			 */
			lparen->tk_flags |= TOKEN_FLAG_TYPE_ARGS;
			peek = 1;
		}
	}
	lexer_peek_leave(lx, &s);

	return peek;
}

/*
 * Fold tokens covered by [src, dst) into a prefix hanging of dst. Any existing
 * prefix hanging of dst after stop (inclusively) will be preserved.
 */
static struct token *
lexer_recover_fold(struct lexer *lx, struct token *src, struct token *dst,
    const struct token *stop)
{
	struct lexer_state st;
	struct token *prefix, *pv;
	size_t beg, end, oldoff;

	pv = src;
	if (!TAILQ_EMPTY(&src->tk_prefixes))
		pv = TAILQ_FIRST(&src->tk_prefixes);

	beg = pv->tk_off;
	end = stop->tk_off;
	memset(&st, 0, sizeof(st));
	st.st_off = beg;
	st.st_lno = pv->tk_lno;
	st.st_cno = pv->tk_cno;
	oldoff = lx->lx_st.st_off;
	lx->lx_st.st_off = end;
	prefix = lexer_emit(lx, &st, &tkcpp);
	lx->lx_st.st_off = oldoff;

	/*
	 * Remove all tokens up to the destination covered by the new prefix
	 * token.
	 */
	for (;;) {
		struct token *nx;

		nx = TAILQ_NEXT(src, tk_entry);
		lexer_trace(lx, "removing %s", token_sprintf(src));
		TAILQ_REMOVE(&lx->lx_tokens, src, tk_entry);
		token_free(src);
		if (nx == dst)
			break;
		src = nx;
	}

	/*
	 * Remove all prefixes hanging of the destination covered by the new
	 * prefix token.
	 */
	while (!TAILQ_EMPTY(&dst->tk_prefixes)) {
		struct token *pr;

		pr = TAILQ_FIRST(&dst->tk_prefixes);
		if (pr == stop)
			break;

		lexer_trace(lx, "removing prefix %s", token_sprintf(pr));
		TAILQ_REMOVE(&dst->tk_prefixes, pr, tk_entry);
		token_free(pr);
	}

	lexer_trace(lx, "add prefix %s to %s", token_sprintf(prefix),
	    token_sprintf(dst));
	TAILQ_INSERT_HEAD(&dst->tk_prefixes, prefix, tk_entry);

	return prefix;
}

static int
lexer_recover_hard(struct lexer *lx, struct token *seek)
{
	if (lx->lx_expect != TOKEN_NONE) {
		struct token *back, *expect, *prefix;
		size_t off;

		/*
		 * Recover by turning everything from the unconsumed token to
		 * the expected token into a prefix.
		 */
		if (!lexer_back(lx, &back) ||
		    !lexer_peek_until(lx, lx->lx_expect, &expect))
			return 0;

		/* Play it safe, do nothing while crossing a line. */
		if (token_cmp(back, expect))
			return 0;

		lexer_trace(lx, "back %s, expect %s", token_sprintf(back),
		    token_sprintf(expect));

		prefix = lexer_recover_fold(lx, back, expect, expect);
		/*
		 * Ugliness ahead, preserve any white space preceding the
		 * prefix.
		 */
		for (off = prefix->tk_off; off > 0; off--) {
			if (!isspace((unsigned char)prefix->tk_str[-1]))
				break;

			prefix->tk_str--;
			prefix->tk_len++;
		}

		lx->lx_expect = TOKEN_NONE;
	} else {
		struct lexer_state s;
		struct token *rparen, *semi;
		int peek = 0;

		/*
		 * Recover from preprocessor declaration lacking a trailing
		 * semicolon by injecting a fake semicolon.
		 */
		lexer_peek_enter(lx, &s);
		if (lexer_if(lx, TOKEN_IDENT, NULL) &&
		    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen) &&
		    !lexer_if(lx, TOKEN_SEMI, NULL))
			peek = 1;
		lexer_peek_leave(lx, &s);
		if (!peek)
			return 0;

		semi = lexer_emit_fake(lx, TOKEN_SEMI, rparen);
		lexer_trace(lx, "added %s after %s", token_sprintf(semi),
		    token_sprintf(rparen));
	}

	lexer_recover_reset(lx, seek);
	return 1;
}

static void
lexer_recover_reset(struct lexer *lx, struct token *seek)
{
	int i;

	for (i = 0; i < NMARKERS; i++)
		lx->lx_markers[i] = NULL;

	lexer_trace(lx, "seek to %s", token_sprintf(seek));
	lx->lx_st.st_tok = TAILQ_PREV(seek, token_list, tk_entry);
	lx->lx_st.st_err = 0;
}

static struct token *
lexer_branch_find(struct token *tk)
{
	for (;;) {
		struct token *br;

		br = token_find_prefix(tk, TOKEN_CPP_IF);
		if (br != NULL)
			return br;

		br = token_find_prefix(tk, TOKEN_CPP_ENDIF);
		if (br != NULL)
			return br->tk_branch.br_pv;

		tk = TAILQ_NEXT(tk, tk_entry);
		if (tk == NULL)
			break;
	}

	return NULL;
}

static struct token *
lexer_branch_next(const struct lexer *lx)
{
	struct token *tk;
	int i;

	if (!lexer_back(lx, &tk))
		return NULL;

	for (i = 0; i < 2; i++) {
		struct token *br;

		br = token_get_branch(tk);
		if (br != NULL)
			return br;

		tk = TAILQ_NEXT(tk, tk_entry);
		if (tk == NULL)
			break;
	}
	return NULL;
}

static void
lexer_branch_enter(struct lexer *lx, struct token *cpp, struct token *tk)
{
	struct branch *br;

	lexer_trace(lx, "%s", token_sprintf(cpp));

	cpp->tk_token = tk;

	br = calloc(1, sizeof(*br));
	if (br == NULL)
		err(1, NULL);
	br->br_cpp = cpp;
	TAILQ_INSERT_TAIL(&lx->lx_branches, br, br_entry);
}

static void
lexer_branch_leave(struct lexer *lx, struct token *cpp, struct token *tk)
{
	struct branch *br;

	br = TAILQ_LAST(&lx->lx_branches, branch_list);
	/* Silently ignore broken branch. */
	if (br == NULL)
		return;

	/*
	 * Discard branches hanging of the same token, such branch cannot cause
	 * removal of any tokens.
	 */
	if (br->br_cpp->tk_token == tk && br->br_cpp->tk_type == TOKEN_CPP_IF) {
		lexer_trace(lx, "%s. discard empty branch", token_sprintf(cpp));
		token_branch_unlink(br->br_cpp);
		token_branch_unlink(cpp);
	} else {
		cpp->tk_token = tk;
		token_branch_link(br->br_cpp, cpp);
		lexer_trace(lx, "%s -> %s", token_sprintf(br->br_cpp),
		    token_sprintf(cpp));
	}

	TAILQ_REMOVE(&lx->lx_branches, br, br_entry);
	free(br);
}

static void
lexer_branch_link(struct lexer *lx, struct token *cpp, struct token *tk)
{
	struct branch *br;

	br = TAILQ_LAST(&lx->lx_branches, branch_list);
	/* Silently ignore broken branch. */
	if (br == NULL)
		return;

	/*
	 * Discard branches hanging of the same token, such branch cannot cause
	 * removal of any tokens.
	 */
	if (br->br_cpp->tk_token == tk) {
		token_branch_unlink(cpp);
		return;
	}

	lexer_trace(lx, "%s -> %s", token_sprintf(br->br_cpp),
	    token_sprintf(cpp));

	cpp->tk_token = tk;
	token_branch_link(br->br_cpp, cpp);
	br->br_cpp = cpp;
}

static void
__lexer_trace(const struct lexer *UNUSED(lx), const char *fun, const char *fmt,
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

static int
token_branch_cover(const struct token *br, const struct token *tk)
{
	return token_cmp(br, tk) < 0 && token_cmp(br->tk_branch.br_nx, tk) > 0;
}

static void
token_branch_link(struct token *src, struct token *dst)
{
	src->tk_branch.br_nx = dst;
	dst->tk_branch.br_pv = src;
}

static void
token_branch_unlink(struct token *tk)
{
	struct token *nx, *pv;

	pv = tk->tk_branch.br_pv;
	nx = tk->tk_branch.br_nx;

	if (tk->tk_type == TOKEN_CPP_IF) {
		if (nx != NULL) {
			nx->tk_branch.br_pv = NULL;
			tk->tk_branch.br_nx = NULL;
		} else {
			/* Branch exhausted. */
			tk->tk_type = TOKEN_CPP;
		}
	} else if (tk->tk_type == TOKEN_CPP_ELSE ||
	    tk->tk_type == TOKEN_CPP_ENDIF) {
		if (pv != NULL) {
			pv->tk_branch.br_nx = NULL;
			tk->tk_branch.br_pv = NULL;
		} else if (nx != NULL) {
			nx->tk_branch.br_pv = NULL;
			tk->tk_branch.br_nx = NULL;
		} else {
			/* Branch exhausted. */
			tk->tk_type = TOKEN_CPP;
		}
	}
}

/*
 * Returns the branch continuation associated with the given token if present.
 */
static struct token *
token_get_branch(struct token *tk)
{
	struct token *br;

	br = token_find_prefix(tk, TOKEN_CPP_ELSE);
	if (br == NULL)
		return NULL;
	return br->tk_branch.br_pv;
}

static struct token *
token_find_prefix(const struct token *tk, enum token_type type)
{
	struct token *prefix;

	TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
		if (prefix->tk_type == type)
			return prefix;
	}
	return NULL;
}

static void
token_free(struct token *tk)
{
	if (tk == NULL)
		return;

	token_list_free(&tk->tk_prefixes);
	token_list_free(&tk->tk_suffixes);
	free(tk);
}

static void
token_list_free(struct token_list *tl)
{
	struct token *tmp;

	while ((tmp = TAILQ_FIRST(tl)) != NULL) {
		TAILQ_REMOVE(tl, tmp, tk_entry);
		token_branch_unlink(tmp);
		token_free(tmp);
	}
}

static const char *
strtoken(enum token_type type)
{
	switch (type) {
#define T(t, s, f) case t: return &#t[sizeof("TOKEN_") - 1];
#include "token.h"
	}
	return NULL;
}
