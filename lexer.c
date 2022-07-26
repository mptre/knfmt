#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

#ifdef HAVE_UTHASH
#  include <uthash.h>
#else
#  include "compat-uthash.h"
#endif

#define TAILQ_INSERTED(elm, field) \
	((elm)->field.tqe_next != NULL || (elm)->field.tqe_prev != NULL)

struct branch {
	struct token		*br_cpp;
	TAILQ_ENTRY(branch)	 br_entry;
};

TAILQ_HEAD(branch_list, branch);

struct lexer {
	struct lexer_state	 lx_st;
	struct error		*lx_er;
	const struct config	*lx_cf;
	const struct diff	*lx_diff;
	const struct buffer	*lx_bf;
	const char		*lx_path;

	/* Line number to buffer offset mapping. */
	struct {
		unsigned int	*l_off;
		size_t		 l_len;
		size_t		 l_siz;
	} lx_lines;

	int			 lx_eof;
	int			 lx_peek;

	struct token		*lx_unmute;

	struct token_list	 lx_tokens;
	struct token_list	 lx_stamps;
	struct branch_list	 lx_branches;
};

struct token_hash {
	struct token	th_tk;
	UT_hash_handle	th_hh;
};

static int		 lexer_getc(struct lexer *, unsigned char *);
static void		 lexer_ungetc(struct lexer *);
static int		 lexer_read(struct lexer *, struct token **);
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

static int	lexer_find_token(const struct lexer *,
    const struct lexer_state *, struct token **);
static int	lexer_buffer_streq(const struct lexer *,
    const struct lexer_state *, const char *);

static struct token	*lexer_emit(struct lexer *, const struct lexer_state *,
    const struct token *);
static void		 lexer_emit_error(struct lexer *, enum token_type,
    const struct token *, const char *, int);

static int	lexer_peek_if_func_ptr(struct lexer *, struct token **);
static int	lexer_peek_if_type_ident(struct lexer *lx);
static int	lexer_peek_if_type_cpp(struct lexer *lx);

static void		 lexer_branch_fold(struct lexer *, struct token *);
static struct token	*lexer_branch_find(struct token *, int);
static void		 lexer_branch_enter(struct lexer *, struct token *,
    struct token *);
static void		 lexer_branch_leave(struct lexer *, struct token *,
    struct token *);
static void		 lexer_branch_link(struct lexer *, struct token *,
    struct token *);

#define lexer_trace(lx, fmt, ...) do {					\
	if (TRACE((lx)->lx_cf))						\
		__lexer_trace((lx), __func__, (fmt),			\
		    __VA_ARGS__);					\
} while (0)
static void	__lexer_trace(const struct lexer *, const char *, const char *,
    ...)
	__attribute__((__format__(printf, 3, 4)));

static struct token	*token_alloc(const struct token *);
static void		 token_branch_link(struct token *, struct token *);
static int		 token_branch_unlink(struct token *);
static struct token	*token_get_branch(struct token *);
static void		 token_move_prefix(struct token *, struct token *,
    struct token *);
static struct token	*token_list_find(struct token_list *, enum token_type);
static void		 token_remove(struct token_list *, struct token *);
static void		 token_prolong(struct token *, struct token *);
static const char	*strtoken(enum token_type);

static int	 isnum(unsigned char, int);
static char	*strtrim(const char *, size_t *);

static struct token_hash	*tokens = NULL;

static const struct token	tkcomment = {
	.tk_type	= TOKEN_COMMENT,
	.tk_flags	= TOKEN_FLAG_DANGLING,
};
static const struct token	tkcpp = {
	.tk_type	= TOKEN_CPP,
	.tk_flags	= TOKEN_FLAG_DANGLING | TOKEN_FLAG_CPP,
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

void
token_ref(struct token *tk)
{
	tk->tk_refs++;
}

void
token_rele(struct token *tk)
{
	assert(tk->tk_refs > 0);
	if (--tk->tk_refs > 0)
		return;

	if ((tk->tk_flags & TOKEN_FLAG_DANGLING) == 0) {
		struct token *fix;

		while ((fix = TAILQ_FIRST(&tk->tk_prefixes)) != NULL) {
			token_branch_unlink(fix);
			token_remove(&tk->tk_prefixes, fix);
		}
		while ((fix = TAILQ_FIRST(&tk->tk_suffixes)) != NULL)
			token_remove(&tk->tk_suffixes, fix);
	}
	if (tk->tk_flags & TOKEN_FLAG_DIRTY)
		free((void *)tk->tk_str);
	free(tk);
}

void
token_add_optline(struct token *tk)
{
	struct token *suffix;

	suffix = token_alloc(&tkline);
	suffix->tk_flags |= TOKEN_FLAG_OPTLINE;
	TAILQ_INSERT_TAIL(&tk->tk_suffixes, suffix, tk_entry);
}

/*
 * Returns non-zero if the given token is preceded with whitespace.
 * Such whitespace is never emitted by the lexer we therefore have to resort to
 * inspecting the source code through the underlying lexer buffer.
 */
int
token_has_indent(const struct token *tk)
{
	return tk->tk_off > 0 &&
	    (tk->tk_str[-1] == ' ' || tk->tk_str[-1] == '\t');
}

/*
 * Returns non-zero if the given token has at least nlines number of trailing
 * hard line(s).
 */
int
token_has_line(const struct token *tk, int nlines)
{
	const struct token *suffix;
	unsigned int flags = TOKEN_FLAG_OPTSPACE;

	assert(nlines > 0 && nlines <= 2);

	if (nlines > 1)
		flags |= TOKEN_FLAG_OPTLINE;

	TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
		if (suffix->tk_type == TOKEN_SPACE &&
		    (suffix->tk_flags & flags) == 0)
			return 1;
	}
	return 0;
}

int
token_has_prefix(const struct token *tk, enum token_type type)
{
	struct token_list *list = (struct token_list *)&tk->tk_prefixes;

	return token_list_find(list, type) != NULL;
}

/*
 * Returns non-zero if the given token has trailing tabs.
 */
int
token_has_tabs(const struct token *tk)
{
	const struct token *suffix;

	TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
		if (suffix->tk_type == TOKEN_SPACE &&
		    (suffix->tk_flags & TOKEN_FLAG_OPTSPACE) &&
		    suffix->tk_str[0] == '\t')
			return 1;
	}
	return 0;
}

/*
 * Returns non-zero if the given token has trailing spaces.
 */
int
token_has_spaces(const struct token *tk)
{
	const struct token *suffix;

	TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
		if (suffix->tk_type == TOKEN_SPACE &&
		    (suffix->tk_flags & TOKEN_FLAG_OPTSPACE))
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

	if (tk->tk_type == TOKEN_IDENT) {
		tk = TAILQ_PREV(tk, token_list, tk_entry);
		if (tk == NULL)
			return 0;
	}
	return tk->tk_type == type;
}

/*
 * Returns non-zero if the given token can be moved.
 */
int
token_is_moveable(const struct token *tk)
{
	struct token_list *suffixes = (struct token_list *)&tk->tk_suffixes;
	const struct token *prefix;

	TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
		if (prefix->tk_flags & TOKEN_FLAG_CPP)
			return 0;
	}

	if (token_list_find(suffixes, TOKEN_COMMENT) != NULL)
		return 0;

	return 1;
}

/*
 * Remove all space suffixes from the given token. Returns the number of removed
 * suffixes.
 */
int
token_trim(struct token *tk)
{
	struct token *suffix, *tmp;
	int ntrim = 0;

	TAILQ_FOREACH_SAFE(suffix, &tk->tk_suffixes, tk_entry, tmp) {
		/*
		 * Optional spaces are never emitted and must therefore be
		 * preserved.
		 */
		if (suffix->tk_flags & TOKEN_FLAG_OPTSPACE)
			continue;

		if (suffix->tk_type == TOKEN_SPACE) {
			token_remove(&tk->tk_suffixes, suffix);
			ntrim++;
		}
	}

	return ntrim;
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

		n = snprintf(buf, bufsiz, "%s<%u:%u>(\"%s\")",
		    type, tk->tk_lno, tk->tk_cno, val);
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

void
token_list_copy(struct token_list *src, struct token_list *dst)
{
	struct token *tk;

	TAILQ_FOREACH(tk, src, tk_entry) {
		struct token *cp;

		cp = token_alloc(tk);
		TAILQ_INSERT_TAIL(dst, cp, tk_entry);
	}
}

void
token_list_move(struct token_list *src, struct token_list *dst)
{
	struct token *tk;

	while ((tk = TAILQ_FIRST(src)) != NULL) {
		assert((tk->tk_flags & TOKEN_FLAG_CPP) == 0);
		TAILQ_REMOVE(src, tk, tk_entry);
		TAILQ_INSERT_TAIL(dst, tk, tk_entry);
	}
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
lexer_alloc(const struct file *fe, const struct buffer *bf, struct error *er,
    const struct config *cf)
{
	struct branch *br;
	struct lexer *lx;
	int error = 0;

	lx = calloc(1, sizeof(*lx));
	if (lx == NULL)
		err(1, NULL);
	lx->lx_er = er;
	lx->lx_cf = cf;
	lx->lx_bf = bf;
	lx->lx_diff = &fe->fe_diff;
	lx->lx_path = fe->fe_path;
	lx->lx_st.st_lno = 1;
	lx->lx_st.st_cno = 1;
	TAILQ_INIT(&lx->lx_tokens);
	TAILQ_INIT(&lx->lx_stamps);
	TAILQ_INIT(&lx->lx_branches);
	lexer_line_alloc(lx, 1);

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
		struct token *pv, *tk;

		TAILQ_REMOVE(&lx->lx_branches, br, br_entry);
		tk = br->br_cpp;
		do {
			pv = tk->tk_branch.br_pv;
			lexer_trace(lx, "broken branch: %s%s%s",
			    token_sprintf(tk),
			    pv ? " -> " : "", pv ? token_sprintf(pv) : "");
			while (token_branch_unlink(tk) == 0)
				continue;
			tk = pv;
		} while (tk != NULL);
		free(br);
	}

	if (error) {
		lexer_free(lx);
		return NULL;
	}

	if (UNLIKELY(cf->cf_verbose >= 3))
		lexer_dump(lx);

	return lx;
}

void
lexer_free(struct lexer *lx)
{
	struct token *tk;

	if (lx == NULL)
		return;

	if (lx->lx_unmute != NULL)
		token_rele(lx->lx_unmute);

	while ((tk = TAILQ_FIRST(&lx->lx_stamps)) != NULL) {
		TAILQ_REMOVE(&lx->lx_stamps, tk, tk_stamp);
		token_rele(tk);
	}
	while ((tk = TAILQ_FIRST(&lx->lx_tokens)) != NULL) {
		TAILQ_REMOVE(&lx->lx_tokens, tk, tk_entry);
		assert(tk->tk_refs == 1);
		token_rele(tk);
	}
	free(lx->lx_lines.l_off);
	free(lx);
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

	if ((lx->lx_cf->cf_flags & CONFIG_FLAG_DIFFPARSE) == 0)
		return 0;

	bo = lx->lx_lines.l_off[beg - 1];
	if (end == 0)
		eo = bf->bf_len;
	else
		eo = lx->lx_lines.l_off[end - 1];
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

	tk = lx->lx_st.st_tok;
	if (tk != NULL && !TAILQ_INSERTED(tk, tk_stamp)) {
		if (TRACE(lx->lx_cf)) {
			char *str;

			str = token_sprintf(tk);
			lexer_trace(lx, "stamp %s", str);
			free(str);
		}
		token_ref(tk);
		TAILQ_INSERT_TAIL(&lx->lx_stamps, tk, tk_stamp);
	}
}

/*
 * Try to recover after encountering invalid code. Returns the index of the
 * stamped token seeked to, starting from the end. This index should correspond
 * to the number of documents that must be removed since we're about to parse
 * them again.
 */
int
lexer_recover(struct lexer *lx)
{
	struct token *back, *br, *dst, *seek, *src, *tmp;
	int ndocs = 1;

	/*
	 * Find the first branch by looking backward and forward from the back
	 * token. Note, we could be inside a branch.
	 */
	back = lx->lx_st.st_tok != NULL ?
	    lx->lx_st.st_tok : TAILQ_FIRST(&lx->lx_tokens);
	lexer_trace(lx, "back %s", token_sprintf(back));
	br = lexer_branch_find(back, 0);
	if (br == NULL)
		br = lexer_branch_find(back, 1);
	if (br == NULL)
		return 0;

	src = br->tk_token;
	dst = br->tk_branch.br_nx->tk_token;
	lexer_trace(lx, "branch from %s to %s covering [%s, %s)",
	    token_sprintf(br), token_sprintf(br->tk_branch.br_nx),
	    token_sprintf(src), token_sprintf(dst));

	/*
	 * Find the offset of the first stamped token before the branch.
	 * Must be done before getting rid of the branch as stamped tokens might
	 * be removed.
	 */
	TAILQ_FOREACH_REVERSE(tmp, &lx->lx_stamps, token_list, tk_stamp) {
		if (token_cmp(tmp, br) < 0)
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
	TAILQ_FOREACH_REVERSE(seek, &lx->lx_stamps, token_list, tk_stamp) {
		if (token_cmp(seek, br) < 0)
			break;
	}
	token_rele(br);

	lexer_trace(lx, "seek to %s, removing %d document(s)",
	    token_sprintf(seek ? seek : TAILQ_FIRST(&lx->lx_tokens)),
	    ndocs);
	lx->lx_st.st_tok = seek;
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
	    token_sprintf(br), token_sprintf(br->tk_branch.br_nx),
	    token_sprintf(br->tk_token),
	    token_sprintf(br->tk_branch.br_nx->tk_token));

	token_branch_unlink(br);

	rm = br->tk_token;
	for (;;) {
		struct token *nx;

		lexer_trace(lx, "removing %s", token_sprintf(rm));

		nx = TAILQ_NEXT(rm, tk_entry);
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
	}
	dst->tk_flags |= TOKEN_FLAG_UNMUTE;
	lx->lx_unmute = dst;
	token_ref(lx->lx_unmute);

	/* Rewind to last stamped token. */
	seek = TAILQ_LAST(&lx->lx_stamps, token_list);
	lexer_trace(lx, "seek to %s",
	    token_sprintf(seek ? seek : TAILQ_FIRST(&lx->lx_tokens)));
	lx->lx_st.st_tok = seek;
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

	if (TAILQ_EMPTY(&lx->lx_tokens))
		return 0;

	if (st->st_tok == NULL) {
		st->st_tok = TAILQ_FIRST(&lx->lx_tokens);
	} else if (st->st_tok->tk_type != TOKEN_EOF) {
		struct token *br;

		/* Do not move passed a branch. */
		if (lx->lx_peek == 0 && token_is_branch(st->st_tok))
			return 0;

		st->st_tok = TAILQ_NEXT(st->st_tok, tk_entry);
		if (st->st_tok == NULL)
			goto out;
		br = token_get_branch(st->st_tok);
		if (br == NULL)
			goto out;

		if (lx->lx_peek == 0) {
			/* While not peeking, instruct the parser to halt. */
			lexer_trace(lx, "halt %s", token_sprintf(st->st_tok));
			return 0;
		} else {
			/* While peeking, act as taking the current branch. */
			while (br->tk_branch.br_nx != NULL)
				br = br->tk_branch.br_nx;
			st->st_tok = br->tk_token;
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

struct token *
lexer_copy_after(struct lexer *lx, struct token *after, const struct token *src)
{
	struct token *tk;

	tk = token_alloc(src);
	TAILQ_INSERT_AFTER(&lx->lx_tokens, after, tk, tk_entry);
	return tk;
}

struct token *
lexer_insert_before(struct lexer *UNUSED(lx), struct token *before,
    enum token_type type, const char *str)
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
lexer_insert_after(struct lexer *lx, struct token *after, enum token_type type,
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

void
lexer_remove(struct lexer *lx, struct token *tk, int keepfixes)
{
	struct token *fix;

	assert(tk->tk_type != TOKEN_EOF);

	if (keepfixes) {
		struct token *nx, *pv;

		/*
		 * Next token must always be present, we're in trouble while
		 * trying to remove the EOF token which is the only one lacking
		 * a next token.
		 */
		nx = TAILQ_NEXT(tk, tk_entry);
		assert(nx != NULL);
		while (!TAILQ_EMPTY(&tk->tk_prefixes)) {
			fix = TAILQ_LAST(&tk->tk_prefixes, token_list);
			token_move_prefix(fix, tk, nx);
		}

		pv = TAILQ_PREV(tk, token_list, tk_entry);
		if (pv == NULL)
			pv = nx;
		while (!TAILQ_EMPTY(&tk->tk_suffixes)) {
			fix = TAILQ_FIRST(&tk->tk_suffixes);
			TAILQ_REMOVE(&tk->tk_suffixes, fix, tk_entry);
			TAILQ_INSERT_TAIL(&pv->tk_suffixes, fix, tk_entry);
		}
	} else {
		TAILQ_FOREACH(fix, &tk->tk_prefixes, tk_entry)
			token_branch_unlink(fix);
	}

	if (TAILQ_INSERTED(tk, tk_stamp)) {
		TAILQ_REMOVE(&lx->lx_stamps, tk, tk_stamp);
		token_rele(tk);
	}

	token_remove(&lx->lx_tokens, tk);
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
	lexer_emit_error(lx, type, t, fun, lno);
	if (!lexer_is_branch(lx))
		lx->lx_st.st_tok = pv;
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
lexer_peek_if_type(struct lexer *lx, struct token **tk, unsigned int flags)
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
		if (lexer_peek_if(lx, TOKEN_EOF, NULL))
			break;

		if (lexer_if_flags(lx,
		    TOKEN_FLAG_TYPE | TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE,
		    &t)) {
			if (t->tk_flags & TOKEN_FLAG_IDENT)
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
			if ((flags &
			    (LEXER_TYPE_FLAG_CAST | LEXER_TYPE_FLAG_ARG)) &&
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
			if ((flags & LEXER_TYPE_FLAG_CAST) == 0 &&
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
		} else {
			unknown = 1;
			break;
		}

		ntokens++;
	}
	lexer_peek_leave(lx, &s);

	if (ntokens == 1 &&
	    (flags & LEXER_TYPE_FLAG_ARG) == 0 &&
	    (beg->tk_flags & (TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE))) {
		/* A single qualifier or storage token cannot denote a type. */
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
	t = TAILQ_NEXT(t, tk_entry);
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
 * Consume token(s) until the given token type is encountered. Returns non-zero
 * if such token is found.
 */
int
lexer_until(struct lexer *lx, enum token_type type, struct token **tk)
{
	for (;;) {
		struct token *t;

		if (!lexer_pop(lx, &t) || t->tk_type == TOKEN_EOF)
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
lexer_get_diffchunk(const struct lexer *lx, unsigned int beg)
{
	const struct diffchunk *du;

	TAILQ_FOREACH(du, &lx->lx_diff->di_chunks, du_entry) {
		if (beg >= du->du_beg && beg <= du->du_end)
			return du;
	}
	return NULL;
}

/*
 * Looks unused but only used while debugging and therefore not declared static.
 */
void
lexer_dump(const struct lexer *lx)
{
	struct token *tk;
	unsigned int i = 0;

	TAILQ_FOREACH(tk, &lx->lx_tokens, tk_entry) {
		struct token *prefix, *suffix;
		char *str;
		int nfixes = 0;

		i++;

		TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
			str = token_sprintf(prefix);
			fprintf(stderr, "[L] %-6u   prefix %s", i, str);
			free(str);

			if (prefix->tk_branch.br_pv != NULL) {
				str = token_sprintf(prefix->tk_branch.br_pv);
				fprintf(stderr, ", pv %s", str);
				free(str);
			}
			if (prefix->tk_branch.br_nx != NULL) {
				str = token_sprintf(prefix->tk_branch.br_nx);
				fprintf(stderr, ", nx %s", str);
			}
			fprintf(stderr, "\n");
			nfixes++;
		}

		str = token_sprintf(tk);
		fprintf(stderr, "[L] %-6u %s\n", i, str);
		free(str);

		TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
			str = token_sprintf(suffix);
			fprintf(stderr, "[L] %-6u   suffix %s\n", i, str);
			free(str);
			nfixes++;
		}

		if (nfixes > 0)
			fprintf(stderr, "[L] -\n");
	}
}

static int
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
	struct token_list prefixes;
	struct token *t, *tmp;
	int error = 0;
	int ncomments = 0;
	int nlines;
	unsigned char ch;

	TAILQ_INIT(&prefixes);
	st = lx->lx_st;

	if (lx->lx_st.st_err)
		goto err;

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
		    last->tk_type == TOKEN_COMMENT)
			token_prolong(last, tmp);
		else
			TAILQ_INSERT_TAIL(&prefixes, tmp, tk_entry);
	}

	if ((*tk = lexer_keyword(lx)) != NULL)
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
	TAILQ_CONCAT(&(*tk)->tk_prefixes, &prefixes, tk_entry);

	/*
	 * Consume trailing/interwined comments, will be hanging of the emitted
	 * token.
	 */
	for (;;) {
		if ((tmp = lexer_comment(lx, 0)) == NULL)
			break;
		TAILQ_INSERT_TAIL(&(*tk)->tk_suffixes, tmp, tk_entry);
		ncomments++;
	}

	/*
	 * Trailing whitespace is only honored if it's present immediately after
	 * the token.
	 */
	if (ncomments == 0 && lexer_eat_spaces(lx, &tmp)) {
		tmp->tk_flags |= TOKEN_FLAG_OPTSPACE;
		TAILQ_INSERT_TAIL(&(*tk)->tk_suffixes, tmp, tk_entry);
	}

	/* Consume hard lines, will be hanging of the emitted token. */
	if ((nlines = lexer_eat_lines(lx, &tmp, 0)) > 0) {
		if (nlines == 1)
			tmp->tk_flags |= TOKEN_FLAG_OPTLINE;
		TAILQ_INSERT_TAIL(&(*tk)->tk_suffixes, tmp, tk_entry);
	}

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
	char *str;
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
	if ((str = strtrim(tk->tk_str, &tk->tk_len)) != NULL) {
		tk->tk_str = str;
		tk->tk_flags |= TOKEN_FLAG_DIRTY;
	}

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
	char *str;
	enum token_type type = TOKEN_CPP;
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

	/* Consume up to 2 hard line(s), will be hanging of the cpp token. */
	lexer_eat_lines(lx, NULL, 2);

	cpp = tkcpp;
	cpp.tk_type = type;
	tk = lexer_emit(lx, &st, &cpp);
	if ((str = strtrim(tk->tk_str, &tk->tk_len)) != NULL) {
		tk->tk_str = str;
		tk->tk_flags |= TOKEN_FLAG_DIRTY;
	}

	/* Discard any remaining hard line(s). */
	lexer_eat_lines(lx, NULL, 0);

	return tk;
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

static void
lexer_line_alloc(struct lexer *lx, unsigned int lno)
{
	size_t siz;

	if ((lx->lx_cf->cf_flags & CONFIG_FLAG_DIFFPARSE) == 0)
		return;

	if (lno - 1 < lx->lx_lines.l_len)
		return;

	siz = lx->lx_lines.l_siz;
	if (lx->lx_lines.l_len + 1 >= siz) {
		size_t newsiz;

		newsiz = siz == 0 ? 1024 : lx->lx_lines.l_siz * 2;
		lx->lx_lines.l_off = reallocarray(lx->lx_lines.l_off, newsiz,
		    sizeof(*lx->lx_lines.l_off));
		if (lx->lx_lines.l_off == NULL)
			err(1, NULL);
		lx->lx_lines.l_siz = newsiz;
	}

	lx->lx_lines.l_off[lx->lx_lines.l_len++] = lx->lx_st.st_off;
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

static struct token *
lexer_emit(struct lexer *lx, const struct lexer_state *st,
    const struct token *tk)
{
	struct token *t;

	t = token_alloc(tk);
	t->tk_off = st->st_off;
	t->tk_lno = st->st_lno;
	t->tk_cno = st->st_cno;
	if (diff_covers(lx->lx_diff, t->tk_lno))
		t->tk_flags |= TOKEN_FLAG_DIFF;
	if (t->tk_str == NULL) {
		t->tk_str = &lx->lx_bf->bf_ptr[st->st_off];
		t->tk_len = lx->lx_st.st_off - st->st_off;
	}
	if ((t->tk_flags & TOKEN_FLAG_DANGLING) == 0)
		TAILQ_INSERT_TAIL(&lx->lx_tokens, t, tk_entry);
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
			     lexer_if(lx, TOKEN_EOF, NULL))) {
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
	struct token *nx = NULL;
	struct token *ident;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if(lx, TOKEN_LPAREN, NULL) &&
	    lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if(lx, TOKEN_RPAREN, NULL) &&
	    (lexer_if(lx, TOKEN_IDENT, &nx) ||
	     lexer_if(lx, TOKEN_STAR, NULL))) {
		/* Ugly, do not confuse cppx. */
		peek = nx == NULL ||
		    ident->tk_len != nx->tk_len ||
		    strncmp(ident->tk_str, nx->tk_str, nx->tk_len) != 0;
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
	unsigned int flags = 0;

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
		lexer_trace(lx, "removing prefix %s",
		    token_sprintf(pr));
		TAILQ_REMOVE(&dst->tk_token->tk_prefixes, pr, tk_entry);
		/* Completely unlink any branch. */
		while (token_branch_unlink(pr) == 0)
			continue;
		token_rele(pr);
		if (pr == dst)
			break;
	}

	lexer_trace(lx, "add prefix %s to %s", token_sprintf(prefix),
	    token_sprintf(dst->tk_token));
	TAILQ_INSERT_HEAD(&dst->tk_token->tk_prefixes, prefix, tk_entry);

	/*
	 * Keep any existing prefix not covered by the new prefix token
	 * by moving them to the destination.
	 */
	pv = TAILQ_PREV(src, token_list, tk_entry);
	for (;;) {
		struct token *tmp;

		if (pv == NULL)
			break;

		lexer_trace(lx, "keeping prefix %s", token_sprintf(pv));
		tmp = TAILQ_PREV(pv, token_list, tk_entry);
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

		nx = TAILQ_NEXT(rm, tk_entry);
		lexer_trace(lx, "removing %s", token_sprintf(rm));
		flags |= rm->tk_flags & TOKEN_FLAG_UNMUTE;
		lexer_remove(lx, rm, 0);
		rm = nx;
	}

	/* Propagate preserved flags. */
	dst->tk_token->tk_flags |= flags;

	token_rele(dst);
}

static struct token *
lexer_branch_find(struct token *tk, int next)
{
	for (;;) {
		struct token *prefix;

		TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
			if (prefix->tk_type == TOKEN_CPP_IF)
				return prefix;
			if (prefix->tk_type == TOKEN_CPP_ENDIF)
				return prefix->tk_branch.br_pv;
		}

		if (next)
			tk = TAILQ_NEXT(tk, tk_entry);
		else
			tk = TAILQ_PREV(tk, token_list, tk_entry);
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
	if (br == NULL) {
		token_branch_unlink(cpp);
		return;
	}

	/*
	 * Discard branches hanging of the same token, such branch cannot cause
	 * removal of any tokens.
	 */
	if (br->br_cpp->tk_token == tk) {
		struct token *pv;

		lexer_trace(lx, "%s -> %s. discard empty branch",
		    token_sprintf(br->br_cpp), token_sprintf(cpp));

		/*
		 * Prevent the previous branch from being exhausted if we're
		 * about to link it again below.
		 */
		pv = br->br_cpp->tk_branch.br_pv;
		if (pv != NULL)
			br->br_cpp->tk_branch.br_pv = NULL;
		token_branch_unlink(br->br_cpp);

		/*
		 * If this is an empty else branch, try to link with the
		 * previous one instead.
		 */
		br->br_cpp = pv;
	}

	if (br->br_cpp != NULL) {
		cpp->tk_token = tk;
		token_branch_link(br->br_cpp, cpp);
		lexer_trace(lx, "%s -> %s", token_sprintf(br->br_cpp),
		    token_sprintf(cpp));
	} else {
		token_branch_unlink(cpp);
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
	if (br == NULL) {
		token_branch_unlink(cpp);
		return;
	}

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

static struct token *
token_alloc(const struct token *def)
{
	struct token *tk;

	tk = calloc(1, sizeof(*tk));
	if (tk == NULL)
		err(1, NULL);
	if (def != NULL)
		*tk = *def;
	tk->tk_refs = 1;
	TAILQ_INIT(&tk->tk_prefixes);
	TAILQ_INIT(&tk->tk_suffixes);
	return tk;
}

static void
token_branch_link(struct token *src, struct token *dst)
{
	src->tk_branch.br_nx = dst;
	dst->tk_branch.br_pv = src;
}

/*
 * Unlink any branch associated with the given token. Returns one of the
 * following:
 *
 *     1    Branch completely unlinked.
 *     0    Branch not completely unlinked.
 *     -1   Not a branch token.
 */
static int
token_branch_unlink(struct token *tk)
{
	struct token *nx, *pv;

	pv = tk->tk_branch.br_pv;
	nx = tk->tk_branch.br_nx;

	if (tk->tk_type == TOKEN_CPP_IF) {
		if (nx != NULL)
			token_branch_unlink(nx);
		/* Branch exhausted. */
		tk->tk_type = TOKEN_CPP;
		return 1;
	} else if (tk->tk_type == TOKEN_CPP_ELSE ||
	    tk->tk_type == TOKEN_CPP_ENDIF) {
		if (pv != NULL) {
			pv->tk_branch.br_nx = NULL;
			tk->tk_branch.br_pv = NULL;
			if (pv->tk_type == TOKEN_CPP_IF)
				token_branch_unlink(pv);
			pv = NULL;
		} else if (nx != NULL) {
			nx->tk_branch.br_pv = NULL;
			tk->tk_branch.br_nx = NULL;
			if (nx->tk_type == TOKEN_CPP_ENDIF)
				token_branch_unlink(nx);
			nx = NULL;
		}
		if (pv == NULL && nx == NULL) {
			/* Branch exhausted. */
			tk->tk_type = TOKEN_CPP;
			return 1;
		}
		return 0;
	}
	return -1;
}

/*
 * Returns the branch continuation associated with the given token if present.
 */
static struct token *
token_get_branch(struct token *tk)
{
	struct token *br;

	br = token_list_find(&tk->tk_prefixes, TOKEN_CPP_ELSE);
	if (br == NULL)
		return NULL;
	return br->tk_branch.br_pv;
}

/*
 * Associated the given prefix token with another token.
 */
static void
token_move_prefix(struct token *prefix, struct token *src, struct token *dst)
{
	TAILQ_REMOVE(&src->tk_prefixes, prefix, tk_entry);
	TAILQ_INSERT_HEAD(&dst->tk_prefixes, prefix, tk_entry);

	switch (prefix->tk_type) {
	case TOKEN_CPP_IF:
	case TOKEN_CPP_ELSE:
	case TOKEN_CPP_ENDIF: {
		struct token *nx = prefix->tk_branch.br_nx;

		assert(prefix->tk_token == src);

		if (nx != NULL && nx->tk_token == dst) {
			/* Discard empty branch. */
			while (token_branch_unlink(prefix) == 0)
				continue;
		} else {
			prefix->tk_token = dst;
		}
		break;
	}

	default:
		break;
	}
}

static struct token *
token_list_find(struct token_list *list, enum token_type type)
{
	struct token *tk;

	TAILQ_FOREACH(tk, list, tk_entry) {
		if (tk->tk_type == type)
			return tk;
	}
	return NULL;
}

static void
token_remove(struct token_list *tl, struct token *tk)
{
	TAILQ_REMOVE(tl, tk, tk_entry);
	token_rele(tk);
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

static int
isnum(unsigned char ch, int prefix)
{
	if (prefix)
		return isdigit(ch);

	ch = tolower(ch);
	return isdigit(ch) || isxdigit(ch) || ch == 'l' || ch == 'x' ||
	    ch == 'u' || ch == '.';
}

/*
 * Trim trailing whitespace from each line in src. Returns NULL if no such
 * whitespace is found, an optimization for the common case.
 */
static char *
strtrim(const char *src, size_t *srclen)
{
	struct buffer *bf = NULL;
	char *dst;
	size_t len = *srclen;
	size_t co = 0;	/* copy offset in src */
	size_t so = 0;	/* current start offset in src */

	while (len > 0) {
		const char *nl;
		size_t eo;
		int nspaces = 0;

		nl = memchr(&src[so], '\n', len);
		if (nl == NULL)
			break;
		eo = nl - src;
		for (; eo > 0; eo--) {
			if (src[eo - 1] == ' ' || src[eo - 1] == '\t')
				nspaces++;
			else
				break;
		}
		if (nspaces == 0)
			goto next;

		if (bf == NULL)
			bf = buffer_alloc(*srclen);
		buffer_append(bf, &src[co], eo - co);
		buffer_appendc(bf, '\n');
		co = (nl - src) + 1;

next:
		len -= (nl - &src[so]) + 1;
		so += (nl - &src[so]) + 1;
	}

	if (bf == NULL)
		return NULL;

	if (so - co > 1)
		buffer_append(bf, &src[co], so - co);
	*srclen = bf->bf_len;
	dst = buffer_release(bf);
	buffer_free(bf);
	return dst;
}
