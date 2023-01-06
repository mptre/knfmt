#include "token.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "buffer.h"
#include "lexer.h"
#include "util.h"

static struct token	*token_list_find(struct token_list *, int);

static void		 strflags(struct buffer *, unsigned int);
static const char	*strtype(int);

#ifdef HAVE_QUEUE
#  include <sys/queue.h>
#else
#  include "compat-queue.h"
#endif

struct token *
token_alloc(const struct token *def)
{
	struct token *tk;

	tk = ecalloc(1, sizeof(*tk));
	if (def != NULL)
		*tk = *def;
	tk->tk_refs = 1;
	TAILQ_INIT(&tk->tk_prefixes);
	TAILQ_INIT(&tk->tk_suffixes);
	return tk;
}

void
token_ref(struct token *tk)
{
	tk->tk_refs++;
}

void
token_rele(struct token *tk)
{
	struct token *fix;

	assert(tk->tk_refs > 0);
	if (--tk->tk_refs > 0)
		return;

	while ((fix = TAILQ_FIRST(&tk->tk_prefixes)) != NULL) {
		token_branch_unlink(fix);
		token_list_remove(&tk->tk_prefixes, fix);
	}
	while ((fix = TAILQ_FIRST(&tk->tk_suffixes)) != NULL)
		token_list_remove(&tk->tk_suffixes, fix);
	free(tk);
}

void
token_add_optline(struct token *tk)
{
	struct token *suffix;

	suffix = token_alloc(&(struct token){
	    .tk_type	= TOKEN_SPACE,
	    .tk_str	= "\n",
	    .tk_len	= 1,
	});
	suffix->tk_flags |= TOKEN_FLAG_OPTLINE;
	TAILQ_INSERT_TAIL(&tk->tk_suffixes, suffix, tk_entry);
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
			token_list_remove(&tk->tk_suffixes, suffix);
			ntrim++;
		}
	}
	if (ntrim > 0)
		tk->tk_flags |= TOKEN_FLAG_TRIMMED;

	return ntrim;
}

char *
token_serialize(const struct token *tk)
{
	struct buffer *bf;
	char *buf;

	bf = buffer_alloc(128);
	buffer_printf(bf, "%s", strtype(tk->tk_type));
	if (tk->tk_str != NULL) {
		buffer_printf(bf, "<%u:%u", tk->tk_lno, tk->tk_cno);
		strflags(bf, tk->tk_flags);
		buffer_printf(bf, ">(\"");
		strnice_buffer(bf, tk->tk_str, tk->tk_len);
		buffer_printf(bf, "\")");
	}
	buffer_putc(bf, '\0');
	buf = buffer_release(bf);
	buffer_free(bf);
	return buf;
}

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

int
token_has_prefix(const struct token *tk, int type)
{
	struct token_list *list = (struct token_list *)&tk->tk_prefixes;

	return token_list_find(list, type) != NULL;
}

int
token_has_suffix(const struct token *tk, int type)
{
	struct token_list *list = (struct token_list *)&tk->tk_suffixes;

	return token_list_find(list, type) != NULL;
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

	if (nlines == 1 && (tk->tk_flags & TOKEN_FLAG_TRIMMED))
		return 1;

	if (nlines > 1)
		flags |= TOKEN_FLAG_OPTLINE;

	TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
		if (suffix->tk_type == TOKEN_SPACE &&
		    (suffix->tk_flags & flags) == 0)
			return 1;
	}
	return 0;
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
 * Returns non-zero if the given token has trailing spaces, including tabs.
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

int
token_has_c99_comment(const struct token *tk)
{
	const struct token *suffix;

	TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
		if (suffix->tk_type == TOKEN_COMMENT &&
		    (suffix->tk_flags & TOKEN_FLAG_COMMENT_C99))
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
token_is_decl(const struct token *tk, int type)
{
	const struct token *nx;

	nx = token_next(tk);
	if (nx == NULL || nx->tk_type != TOKEN_LBRACE)
		return 0;

	if (tk->tk_type == TOKEN_IDENT) {
		tk = token_prev(tk);
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
	const struct token *prefix, *suffix;

	TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
		if (prefix->tk_type == TOKEN_COMMENT ||
		    (prefix->tk_flags & TOKEN_FLAG_CPP))
			return 0;
	}

	TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
		if (suffix->tk_type == TOKEN_COMMENT)
			return 0;
	}

	return 1;
}

/*
 * Returns the branch continuation associated with the given token if present.
 */
struct token *
token_get_branch(struct token *tk)
{
	struct token *br;

	br = token_list_find(&tk->tk_prefixes, TOKEN_CPP_ELSE);
	if (br == NULL)
		return NULL;
	return br->tk_branch.br_pv;
}

struct token *
token_next(const struct token *tk)
{
	return (struct token *)TAILQ_NEXT(tk, tk_entry);
}

struct token *
token_prev(const struct token *tk)
{
	return (struct token *)TAILQ_PREV(tk, token_list, tk_entry);
}

void
token_list_remove(struct token_list *tl, struct token *tk)
{
	TAILQ_REMOVE(tl, tk, tk_entry);
	token_rele(tk);
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
token_move_prefixes(struct token *src, struct token *dst)
{
	while (!TAILQ_EMPTY(&src->tk_prefixes)) {
		struct token *prefix;

		prefix = TAILQ_LAST(&src->tk_prefixes, token_list);
		token_move_prefix(prefix, src, dst);
	}
}

/*
 * Associated the given prefix token with another token.
 */
void
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

void
token_move_suffixes(struct token *src, struct token *dst)
{
	while (!TAILQ_EMPTY(&src->tk_suffixes)) {
		struct token *suffix;

		suffix = TAILQ_FIRST(&src->tk_suffixes);
		TAILQ_REMOVE(&src->tk_suffixes, suffix, tk_entry);
		TAILQ_INSERT_TAIL(&dst->tk_suffixes, suffix, tk_entry);
	}
}

void
token_move_suffixes_if(struct token *src, struct token *dst, int type)
{
	struct token *suffix, *tmp;

	TAILQ_FOREACH_SAFE(suffix, &src->tk_suffixes, tk_entry, tmp) {
		if (suffix->tk_type != type)
			continue;

		TAILQ_REMOVE(&src->tk_suffixes, suffix, tk_entry);
		TAILQ_INSERT_TAIL(&dst->tk_suffixes, suffix, tk_entry);
	}
}

/*
 * Unlink any branch associated with the given token. Returns one of the
 * following:
 *
 *     1    Branch completely unlinked.
 *     0    Branch not completely unlinked.
 *     -1   Not a branch token.
 */
int
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

unsigned int
token_flags_inherit(const struct token *tk)
{
	return tk->tk_flags & TOKEN_FLAG_DIFF;
}

static struct token *
token_list_find(struct token_list *list, int type)
{
	struct token *tk;

	TAILQ_FOREACH(tk, list, tk_entry) {
		if (tk->tk_type == type)
			return tk;
	}
	return NULL;
}

static void
strflags(struct buffer *bf, unsigned int token_flags)
{
	static const struct {
		const char	*str;
		size_t		 len;
		unsigned int	 flag;
	} flags[] = {
#define F(f, s) { (s), sizeof(s) - 1, (f) }
		F(TOKEN_FLAG_COMMENT_C99,	"C99"),
		F(TOKEN_FLAG_OPTLINE,		"OPTLINE"),
		F(TOKEN_FLAG_OPTSPACE,		"OPTSPACE"),
		F(TOKEN_FLAG_DIFF,		"DIFF"),
#undef F
	};
	size_t nflags = sizeof(flags) / sizeof(flags[0]);
	size_t i;
	int npresent = 0;

	for (i = 0; i < nflags; i++) {
		if ((token_flags & flags[i].flag) == 0)
			continue;

		if (npresent++ == 0)
			buffer_putc(bf, ',');
		else
			buffer_putc(bf, '|');
		buffer_puts(bf, flags[i].str, flags[i].len);
	}
}

static const char *
strtype(int token_type)
{
	switch (token_type) {
#define T(t, s, f) case t: return &#t[sizeof("TOKEN_") - 1];
#define S(t, s, f) T(t, s, f)
#include "token-defs.h"
	}
	if (token_type == LEXER_EOF)
		return "EOF";
	return NULL;
}
