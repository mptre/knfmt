#include "token.h"

#include "config.h"

#include <assert.h>
#include <string.h>

#include "libks/arena-buffer.h"
#include "libks/arena.h"
#include "libks/buffer.h"
#include "libks/list.h"
#include "libks/string.h"

#include "lexer.h"
#include "util.h"

static const char	*token_flags_str(unsigned int, struct arena_scope *);
static const char	*token_type_str(int);

struct token *
token_alloc(struct arena_scope *s, unsigned int priv_size,
    const struct token *def)
{
	struct token *tk;

	tk = arena_calloc(s, 1, sizeof(*tk) + priv_size);
	*tk = *def;
	tk->tk_refs = 1;
	tk->tk_priv_size = priv_size;
	LIST_INIT(&tk->tk_prefixes);
	LIST_INIT(&tk->tk_suffixes);
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

	while ((fix = LIST_FIRST(&tk->tk_prefixes)) != NULL)
		token_list_remove(&tk->tk_prefixes, fix);
	while ((fix = LIST_FIRST(&tk->tk_suffixes)) != NULL)
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

	LIST_FOREACH(suffix, &tk->tk_suffixes) {
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
	return token_serialize_with_extra_flags(tk, flags, NULL, s);
}

const char *
token_serialize_with_extra_flags(const struct token *tk, unsigned int flags,
    const char *extra_flags, struct arena_scope *s)
{
	struct buffer *bf, *serialized_flags;
	int comma = 0;

	serialized_flags = arena_buffer_alloc(s, 128);
	if (flags & TOKEN_SERIALIZE_POSITION) {
		buffer_printf(serialized_flags, "%u:%u",
		    tk->tk_lno, tk->tk_cno);
		comma++;
	}
	if (flags & TOKEN_SERIALIZE_FLAGS) {
		const char *serialized_token_flags;

		serialized_token_flags = token_flags_str(tk->tk_flags, s);
		if (serialized_token_flags[0] != '\0') {
			buffer_printf(serialized_flags, "%s%s",
			    comma++ ? "," : "",
			    serialized_token_flags);
		}
	}
	if (flags & TOKEN_SERIALIZE_REFS) {
		buffer_printf(serialized_flags, "%s%d",
		    comma++ ? "," : "",
		    tk->tk_refs);
	}
	if (flags & TOKEN_SERIALIZE_ADDRESS) {
		buffer_printf(serialized_flags, "%s%p",
		    comma++ ? "," : "",
		    (const void *)tk);
	}
	if (extra_flags != NULL) {
		buffer_printf(serialized_flags, "%s%s",
		    comma++ ? "," : "",
		    extra_flags);
	}

	bf = arena_buffer_alloc(s, 128);
	buffer_printf(bf, "%s", token_type_str(tk->tk_type));
	if (buffer_get_len(serialized_flags) > 0)
		buffer_printf(bf, "<%s>", buffer_str(serialized_flags));

	if (flags & TOKEN_SERIALIZE_VERBATIM) {
		buffer_printf(bf, "(");
		if (flags & TOKEN_SERIALIZE_QUOTE)
			buffer_printf(bf, "\"");
		buffer_printf(bf, "%s", KS_str_vis(tk->tk_str, tk->tk_len, s));
		if (flags & TOKEN_SERIALIZE_QUOTE)
			buffer_printf(bf, "\"");
		buffer_printf(bf, ")");
	}

	return buffer_str(bf);
}

void
token_position_after(struct token *after, struct token *tk)
{
	struct token *last, *suffix;
	unsigned int lno = after->tk_lno;
	unsigned int cno;

	last = LIST_EMPTY(&after->tk_suffixes) ?
	    after : LIST_LAST(&after->tk_suffixes);

	cno = colwidth(last->tk_str, last->tk_len, last->tk_cno);
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
	cno = colwidth(tk->tk_str, tk->tk_len, tk->tk_cno);

	LIST_FOREACH(suffix, &tk->tk_suffixes) {
		suffix->tk_cno = cno;
		suffix->tk_lno = lno;
		cno = colwidth(suffix->tk_str, suffix->tk_len, suffix->tk_cno);
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
	return token_memcmp(a, b->tk_str, b->tk_len);
}

int
token_memcmp(const struct token *tk, const char *str, size_t len)
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

	LIST_FOREACH(prefix, &tk->tk_prefixes) {
		if (prefix->tk_flags & TOKEN_FLAG_CPP)
			return 1;
	}
	return 0;
}

int
token_type_normalize(const struct token *tk)
{
	switch (tk->tk_type) {
#define OP(type, normalized, ...) case type: if (normalized) return normalized; break;
	FOR_TOKEN_CPP(OP)
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

	LIST_FOREACH(suffix, &tk->tk_suffixes) {
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
	return !LIST_EMPTY(&tk->tk_prefixes);
}

/*
 * Returns non-zero if the given token has trailing tabs.
 */
int
token_has_tabs(const struct token *tk)
{
	const struct token *suffix;

	LIST_FOREACH(suffix, &tk->tk_suffixes) {
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

	LIST_FOREACH(prefix, &tk->tk_prefixes) {
		if (prefix->tk_type == TOKEN_COMMENT ||
		    (prefix->tk_flags & TOKEN_FLAG_CPP))
			return 0;
	}

	if (token_list_find(&tk->tk_suffixes, TOKEN_COMMENT, 0) != NULL)
		return 0;

	return 1;
}

/*
 * Returns non-zero if the given token is not part of a token_list.
 */
int
token_is_dangling(const struct token *tk)
{
	return !LIST_LINKED(tk);
}

void
token_set_str(struct token *tk, const char *str, size_t len)
{
	tk->tk_str = str;
	tk->tk_len = len;
}

unsigned int
token_lines(const struct token *tk)
{
	const char *str = tk->tk_str;
	size_t len = tk->tk_len;
	unsigned int nlines = 0;

	while (len > 0) {
		const char *p;
		size_t linelen;

		p = memchr(str, '\n', len);
		if (p == NULL)
			break;
		nlines++;
		linelen = (size_t)(p - str);
		len -= linelen + 1;
		str = p + 1;
	}
	return nlines;
}

void
token_list_prepend(struct token_list *tl, struct token *tk)
{
	LIST_INSERT_HEAD(tl, tk);
}

void
token_list_append(struct token_list *tl, struct token *tk)
{
	LIST_INSERT_TAIL(tl, tk);
}

void
token_list_append_after(struct token_list *tl, struct token *after,
    struct token *tk)
{
	LIST_INSERT_AFTER(tl, after, tk);
}

void
token_list_remove(struct token_list *tl, struct token *tk)
{
	assert(!token_is_dangling(tk));
	LIST_REMOVE(tl, tk);
	token_rele(tk);
}

void
token_list_swap(struct token_list *src, struct token_list *dst)
{
	struct token_list tmp;
	struct token *safe, *tk;

	LIST_INIT(&tmp);

	LIST_FOREACH_SAFE(tk, src, safe) {
		LIST_REMOVE(src, tk);
		LIST_INSERT_TAIL(&tmp, tk);
	}

	LIST_FOREACH_SAFE(tk, dst, safe) {
		LIST_REMOVE(dst, tk);
		LIST_INSERT_TAIL(src, tk);
	}

	LIST_FOREACH_SAFE(tk, &tmp, safe) {
		LIST_REMOVE(&tmp, tk);
		LIST_INSERT_TAIL(dst, tk);
	}
}

struct token *
token_list_first(struct token_list *tl)
{
	return LIST_FIRST(tl);
}

struct token *
token_list_last(struct token_list *tl)
{
	return LIST_LAST(tl);
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
	while (!LIST_EMPTY(&src->tk_suffixes)) {
		struct token *suffix;

		suffix = LIST_FIRST(&src->tk_suffixes);
		LIST_REMOVE(&src->tk_suffixes, suffix);
		LIST_INSERT_TAIL(&dst->tk_suffixes, suffix);
	}
}

void
token_move_suffixes_if(struct token *src, struct token *dst, int type)
{
	struct token *suffix, *tmp;

	LIST_FOREACH_SAFE(suffix, &src->tk_suffixes, tmp) {
		if (suffix->tk_type != type)
			continue;

		LIST_REMOVE(&src->tk_suffixes, suffix);
		LIST_INSERT_TAIL(&dst->tk_suffixes, suffix);
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
		LIST_FOREACH(tk, tokens) {
			if (tk->tk_type == type && (tk->tk_flags & flags))
				return tk;
		}
	} else {
		LIST_FOREACH(tk, tokens) {
			if (tk->tk_type == type)
				return tk;
		}
	}

	return NULL;
}

static const char *
token_flags_str(unsigned int token_flags, struct arena_scope *s)
{
	static const struct {
		const char	*str;
		size_t		 len;
		unsigned int	 flag;
	} flags[] = {
#define F(f, s) { (s), sizeof(s) - 1, (f) }
		F(TOKEN_FLAG_ASSIGN,			"ASSIGN"),
		F(TOKEN_FLAG_BRANCH,			"BRANCH"),
		F(TOKEN_FLAG_DISCARD,			"DISCARD"),
		F(TOKEN_FLAG_COMMENT_C99,		"C99"),
		F(TOKEN_FLAG_COMMENT_CLANG_FORMAT_ON,	"CLANG_FORMAT_ON"),
		F(TOKEN_FLAG_COMMENT_CLANG_FORMAT_OFF,	"CLANG_FORMAT_OFF"),
		F(TOKEN_FLAG_CPP,			"CPP"),
		F(TOKEN_FLAG_OPTLINE,			"OPTLINE"),
		F(TOKEN_FLAG_OPTSPACE,			"OPTSPACE"),
		F(TOKEN_FLAG_DIFF,			"DIFF"),
#undef F
	};
	size_t nflags = sizeof(flags) / sizeof(flags[0]);
	size_t i;
	struct buffer *bf;

	bf = arena_buffer_alloc(s, 1 << 6);

	for (i = 0; i < nflags; i++) {
		if ((token_flags & flags[i].flag) == 0)
			continue;

		if (buffer_get_len(bf) > 0)
			buffer_putc(bf, '|');
		buffer_puts(bf, flags[i].str, flags[i].len);
	}

	return buffer_str(bf);
}

static const char *
token_type_str(int token_type)
{
	switch (token_type) {
#define OP(type, ...) case type: return &#type[sizeof("TOKEN_") - 1];
	FOR_TOKEN_TYPES(OP)
	FOR_TOKEN_CPP(OP)
	FOR_TOKEN_SENTINELS(OP)
#undef OP
	}
	if (token_type == LEXER_EOF)
		return "EOF";
	return NULL;
}
