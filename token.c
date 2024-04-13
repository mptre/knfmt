#include "token.h"

#include "config.h"

#include <assert.h>
#include <string.h>

#include "libks/arena-buffer.h"
#include "libks/arena.h"
#include "libks/buffer.h"

#include "lexer.h"
#include "util.h"

static void		 strflags(struct buffer *, unsigned int);
static const char	*token_type_str(int);

#ifdef HAVE_QUEUE
#  include <sys/queue.h>
#else
#  include "compat-queue.h"
#endif

struct token *
token_alloc(struct arena_scope *s, unsigned int priv_size,
    const struct token *def)
{
	struct token *tk;

	tk = arena_calloc(s, 1, sizeof(*tk) + priv_size);
	*tk = *def;
	tk->tk_refs = 1;
	tk->tk_priv_size = priv_size;
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

	while ((fix = TAILQ_FIRST(&tk->tk_prefixes)) != NULL)
		token_list_remove(&tk->tk_prefixes, fix);
	while ((fix = TAILQ_FIRST(&tk->tk_suffixes)) != NULL)
		token_list_remove(&tk->tk_suffixes, fix);

	arena_poison(tk, sizeof(*tk) + tk->tk_priv_size);
}

/*
 * Remove all space suffixes from the given token. Returns the number of removed
 * suffixes.
 */
int
token_trim(struct token *tk)
{
	struct token *suffix;
	int ntrim = 0;

	TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
		/*
		 * Optional spaces are never emitted and must therefore be
		 * preserved.
		 */
		if (suffix->tk_flags & TOKEN_FLAG_OPTSPACE)
			continue;

		if (suffix->tk_type == TOKEN_SPACE) {
			suffix->tk_flags |= TOKEN_FLAG_DISCARD;
			ntrim++;
		}
	}
	return ntrim;
}

const char *
token_serialize(const struct token *tk, unsigned int flags,
    struct arena_scope *s)
{
	struct buffer *bf;

	bf = arena_buffer_alloc(s, 128);
	buffer_printf(bf, "%s", token_type_str(tk->tk_type));
	if (tk->tk_str != NULL) {
		if (flags > 0)
			buffer_printf(bf, "<");
		if (flags & TOKEN_SERIALIZE_POSITION)
			buffer_printf(bf, "%u:%u", tk->tk_lno, tk->tk_cno);
		if (flags & TOKEN_SERIALIZE_FLAGS)
			strflags(bf, tk->tk_flags);
		if (flags & TOKEN_SERIALIZE_REFS)
			buffer_printf(bf, ",%d", tk->tk_refs);
		if (flags & TOKEN_SERIALIZE_ADDRESS)
			buffer_printf(bf, ",%p", (void *)tk);
		if (flags > 0)
			buffer_printf(bf, ">");
		buffer_printf(bf, "(\"");
		strnice_buffer(bf, tk->tk_str, tk->tk_len);
		buffer_printf(bf, "\")");
	}
	return buffer_str(bf);
}

void
token_position_after(struct token *after, struct token *tk)
{
	struct token *last, *suffix;
	unsigned int lno = after->tk_lno;
	unsigned int cno;

	last = TAILQ_EMPTY(&after->tk_suffixes) ?
	    after : TAILQ_LAST(&after->tk_suffixes, token_list);

	cno = colwidth(last->tk_str, last->tk_len, last->tk_cno, NULL);
	/*
	 * If after is the last token on this line, use the column from the
	 * first token on the same line.
	 */
	if (cno == 1) {
		struct token *pv = after;

		for (;;) {
			struct token *tmp;

			cno = pv->tk_cno;
			tmp = token_prev(pv);
			if (tmp == NULL || tmp->tk_lno != lno)
				break;
			pv = tmp;
		}
	}

	/* Intentionally not adjusting prefixes. */

	tk->tk_cno = cno;
	tk->tk_lno = lno;
	cno = colwidth(tk->tk_str, tk->tk_len, tk->tk_cno, NULL);

	TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
		suffix->tk_cno = cno;
		suffix->tk_lno = lno;
		cno = colwidth(suffix->tk_str, suffix->tk_len, suffix->tk_cno,
		    NULL);
	}
}

int
token_cmp(const struct token *a, const struct token *b)
{
	int gt, le;

	le = a->tk_lno < b->tk_lno;
	gt = a->tk_lno > b->tk_lno;
	/* Intentionally not comparing the column. */
	return (le * -1) + (gt * 1);
}

int
token_strcmp(const struct token *a, const struct token *b)
{
	return token_rawcmp(a, b->tk_str, b->tk_len);
}

int
token_rawcmp(const struct token *tk, const char *str, size_t len)
{
	size_t minlen;
	int cmp, gt, le;

	minlen = tk->tk_len < len ? tk->tk_len : len;
	cmp = strncmp(tk->tk_str, str, minlen);
	if (cmp != 0)
		return cmp;
	le = tk->tk_len < len;
	gt = tk->tk_len > len;
	return (le * -1) + (gt * 1);
}

/*
 * Returns non-zero if the given token preceded with preprocessor directives.
 */
int
token_has_cpp(const struct token *tk)
{
	const struct token *prefix;

	TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
		if (prefix->tk_flags & TOKEN_FLAG_CPP)
			return 1;
	}
	return 0;
}

int
token_type_normalize(const struct token *tk)
{
	switch (tk->tk_type) {
#define OP(type, normalized) case type: if (normalized) return normalized; break;
	FOR_TOKEN_SENTINELS(OP)
#undef OP
	}
	return tk->tk_type;
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
token_has_suffix(const struct token *tk, int type)
{
	return token_list_find(&tk->tk_suffixes, type, 0) != NULL;
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

/*
 * Returns non-zero if the given token has at least nlines number of trailing
 * verbatim hard line(s).
 */
int
token_has_verbatim_line(const struct token *tk, int nlines)
{
	const char *str = tk->tk_str;
	size_t len = tk->tk_len;

	for (; len > 0; len--) {
		if (str[len - 1] != '\n')
			break;
		if (--nlines == 0)
			break;
	}
	return nlines == 0;
}

/*
 * Returns non-zero if the given token has any prefix.
 */
int
token_has_prefixes(const struct token *tk)
{
	return !TAILQ_EMPTY(&tk->tk_prefixes);
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
	return token_list_find(&tk->tk_suffixes, TOKEN_SPACE,
	    TOKEN_FLAG_OPTSPACE) != NULL;
}

int
token_has_c99_comment(const struct token *tk)
{
	return token_list_find(&tk->tk_suffixes,
	    TOKEN_COMMENT, TOKEN_FLAG_COMMENT_C99) != NULL;
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
	const struct token *prefix;

	TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
		if (prefix->tk_type == TOKEN_COMMENT ||
		    (prefix->tk_flags & TOKEN_FLAG_CPP))
			return 0;
	}

	if (token_list_find(&tk->tk_suffixes, TOKEN_COMMENT, 0) != NULL)
		return 0;

	return 1;
}

/*
 * Returns non-zero if the given token is the first one on the line it resides.
 */
int
token_is_first(const struct token *tk)
{
	const struct token *pv;

	pv = token_prev(tk);
	return pv == NULL || pv->tk_lno != tk->tk_lno;
}

/*
 * Returns non-zero if the given token is not part of a token_list.
 */
int
token_is_dangling(const struct token *tk)
{
	static char zero[sizeof(tk->tk_entry)];

	/* Try to not rely on TAILQ_ENTRY() implementation details. */
	return memcmp(&tk->tk_entry, zero, sizeof(zero)) == 0;
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
token_set_str(struct token *tk, const char *str, size_t len)
{
	tk->tk_str = str;
	tk->tk_len = len;
}

void
token_list_prepend(struct token_list *tl, struct token *tk)
{
	TAILQ_INSERT_HEAD(tl, tk, tk_entry);
}

void
token_list_append(struct token_list *tl, struct token *tk)
{
	TAILQ_INSERT_TAIL(tl, tk, tk_entry);
}

void
token_list_append_after(struct token_list *tl, struct token *after,
    struct token *tk)
{
	TAILQ_INSERT_AFTER(tl, after, tk, tk_entry);
}

void
token_list_remove(struct token_list *tl, struct token *tk)
{
	if (!token_is_dangling(tk)) {
		TAILQ_REMOVE(tl, tk, tk_entry);
		memset(&tk->tk_entry, 0, sizeof(tk->tk_entry));
	}
	token_rele(tk);
}

/*
 * Swap the two lists but preserve tokens with the given flags.
 */
void
token_list_swap(struct token_list *src, unsigned int src_token_flags,
    struct token_list *dst, unsigned int dst_token_flags)
{
	struct token_list tmp;
	struct token *before = NULL;
	struct token *safe, *tk;

	TAILQ_INIT(&tmp);
	TAILQ_FOREACH_SAFE(tk, src, tk_entry, safe) {
		tk = TAILQ_FIRST(src);
		if (tk->tk_flags & src_token_flags) {
			if (before == NULL)
				before = tk;
			continue;
		}

		TAILQ_REMOVE(src, tk, tk_entry);
		TAILQ_INSERT_TAIL(&tmp, tk, tk_entry);
	}

	TAILQ_FOREACH_SAFE(tk, dst, tk_entry, safe) {
		if (tk->tk_flags & dst_token_flags)
			continue;

		TAILQ_REMOVE(dst, tk, tk_entry);
		if (before != NULL)
			TAILQ_INSERT_BEFORE(before, tk, tk_entry);
		else
			TAILQ_INSERT_TAIL(src, tk, tk_entry);
	}

	while (!TAILQ_EMPTY(&tmp)) {
		tk = TAILQ_FIRST(&tmp);
		TAILQ_REMOVE(&tmp, tk, tk_entry);
		TAILQ_INSERT_TAIL(dst, tk, tk_entry);
	}
}

struct token *
token_list_first(struct token_list *tl)
{
	return TAILQ_FIRST(tl);
}

struct token *
token_list_last(struct token_list *tl)
{
	return TAILQ_LAST(tl, token_list);
}

struct token *
token_find_suffix_spaces(struct token *tk)
{
	return token_list_find(&tk->tk_suffixes, TOKEN_SPACE,
	    TOKEN_FLAG_OPTSPACE);
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

unsigned int
token_flags_inherit(const struct token *tk)
{
	return tk->tk_flags & TOKEN_FLAG_DIFF;
}

struct token *
token_list_find(const struct token_list *tokens, int type, unsigned int flags)
{
	struct token *tk;

	if (flags > 0) {
		TAILQ_FOREACH(tk, tokens, tk_entry) {
			if (tk->tk_type == type && (tk->tk_flags & flags))
				return tk;
		}
	} else {
		TAILQ_FOREACH(tk, tokens, tk_entry) {
			if (tk->tk_type == type)
				return tk;
		}
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
		F(TOKEN_FLAG_BRANCH,			"BRANCH"),
		F(TOKEN_FLAG_DISCARD,			"DISCARD"),
		F(TOKEN_FLAG_COMMENT_C99,		"C99"),
		F(TOKEN_FLAG_CPP,			"CPP"),
		F(TOKEN_FLAG_OPTLINE,			"OPTLINE"),
		F(TOKEN_FLAG_OPTSPACE,			"OPTSPACE"),
		F(TOKEN_FLAG_DIFF,			"DIFF"),
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
token_type_str(int token_type)
{
	switch (token_type) {
#define OP(type, ...) case type: return &#type[sizeof("TOKEN_") - 1];
	FOR_TOKEN_TYPES(OP)
	FOR_TOKEN_SENTINELS(OP)
#undef OP
	}
	if (token_type == LEXER_EOF)
		return "EOF";
	return NULL;
}
