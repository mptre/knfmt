#include "cpp-include.h"

#include "config.h"

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "buffer.h"
#include "lexer.h"
#include "options.h"
#include "style.h"
#include "token.h"
#include "util.h"
#include "vector.h"

struct cpp_include {
	VECTOR(struct include)	 includes;
	struct lexer		*lx;
	struct token_list	*prefixes;
	struct token		*after;
};

struct include {
	struct token	*tk;
	struct {
		const char	*str;
		size_t		 len;
	} path;
};

static int	is_sort_includes_enabled(const struct options *,
    const struct style *);

static void	cpp_include_exec(struct cpp_include *);
static void	cpp_include_reset(struct cpp_include *);

static void	add_line(struct cpp_include *, struct token *);

static int	include_cmp(const struct include *, const struct include *);

static const char	*findpath(const char *, size_t, size_t *);

static int	token_has_verbatim_line(const struct token *);
static void	token_trim_verbatim_line(struct token *);

struct cpp_include *
cpp_include_alloc(const struct options *op, const struct style *st)
{
	struct cpp_include *ci;

	if (!is_sort_includes_enabled(op, st))
		return NULL;

	ci = ecalloc(1, sizeof(*ci));
	if (VECTOR_INIT(ci->includes) == NULL)
		err(1, NULL);
	return ci;
}

void
cpp_include_free(struct cpp_include *ci)
{
	if (ci == NULL)
		return;

	cpp_include_reset(ci);
	VECTOR_FREE(ci->includes);
	free(ci);
}

void
cpp_include_enter(struct cpp_include *ci, struct lexer *lx,
    struct token_list *prefixes)
{
	if (ci == NULL)
		return;

	assert(ci->lx == NULL);
	ci->lx = lx;
	assert(ci->prefixes == NULL);
	ci->prefixes = prefixes;
}

void
cpp_include_leave(struct cpp_include *ci)
{
	if (ci == NULL)
		return;

	cpp_include_exec(ci);
	cpp_include_reset(ci);
	ci->lx = NULL;
	ci->prefixes = NULL;
}

void
cpp_include_add(struct cpp_include *ci, struct token *tk)
{
	if (ci == NULL)
		return;

	if (tk->tk_type == TOKEN_CPP_INCLUDE) {
		struct include *include;

		if (VECTOR_EMPTY(ci->includes))
			ci->after = token_prev(tk);
		include = VECTOR_ALLOC(ci->includes);
		if (include == NULL)
			err(1, NULL);
		token_ref(tk);
		include->tk = tk;
		if (!token_has_verbatim_line(tk))
			return;
	}

	cpp_include_exec(ci);
	cpp_include_reset(ci);
}

static int
is_sort_includes_enabled(const struct options *op, const struct style *st)
{
	return (op->op_flags & OPTIONS_SIMPLE) ||
	    style(st, SortIncludes) == CaseSensitive;
}

static void
cpp_include_exec(struct cpp_include *ci)
{
	struct token *after = ci->after;
	size_t i, nincludes;
	unsigned int nbrackets = 0;
	unsigned int nquotes = 0;
	unsigned int nslashes = 0;
	int doline;

	nincludes = VECTOR_LENGTH(ci->includes);
	if (nincludes < 2)
		return;

	for (i = 0; i < nincludes; i++) {
		struct include *include = &ci->includes[i];
		const struct token *tk = include->tk;
		const char *path;
		size_t len;

		path = findpath(tk->tk_str, tk->tk_len, &len);
		if (path == NULL)
			return;
		include->path.str = path;
		include->path.len = len;

		if (path[0] == '<')
			nbrackets++;
		if (path[0] == '"')
			nquotes++;
		if (memchr(path, '/', len) != NULL)
			nslashes++;
		if ((nbrackets > 0 && nquotes > 0) || nslashes > 0)
			return;
	}

	doline = token_has_verbatim_line(VECTOR_LAST(ci->includes)->tk);

	VECTOR_SORT(ci->includes, include_cmp);

	for (i = 0; i < nincludes; i++) {
		struct include *include = &ci->includes[i];

		token_trim_verbatim_line(include->tk);
		token_list_remove(ci->prefixes, include->tk);
		token_ref(include->tk);
		if (after != NULL) {
			token_list_insert_after(ci->prefixes, after,
			    include->tk);
		} else {
			token_list_insert(ci->prefixes, include->tk);
		}
		after = include->tk;
	}
	if (doline)
		add_line(ci, after);
}

static void
cpp_include_reset(struct cpp_include *ci)
{
	while (!VECTOR_EMPTY(ci->includes)) {
		struct include *include;

		include = VECTOR_POP(ci->includes);
		token_rele(include->tk);
	}
	ci->after = NULL;
}

static void
add_line(struct cpp_include *ci, struct token *after)
{
	struct lexer_state st;
	struct lexer *lx = ci->lx;
	struct token *tk;

	st = lexer_get_state(lx);
	tk = lexer_emit(lx, &st, &(struct token){
	    .tk_type	= TOKEN_SPACE,
	    .tk_str	= "\n",
	    .tk_len	= 1,
	});
	token_list_insert_after(ci->prefixes, after, tk);
}

static int
include_cmp(const struct include *a, const struct include *b)
{
	return token_strcmp(a->tk, b->tk);
}

static const char *
findpath(const char *str, size_t len, size_t *pathlen)
{
	const char *eo, *so;
	char c;

	so = memchr(str, '"', len);
	if (so == NULL)
		so = memchr(str, '<', len);
	if (so == NULL)
		return NULL;
	c = so[0] == '"' ? '"' : '>';
	len -= (size_t)(so - str);
	eo = memchr(&so[1], c, len);
	if (eo == NULL)
		return NULL;
	*pathlen = (size_t)(eo - so) + 1;
	return so;
}

static int
token_has_verbatim_line(const struct token *tk)
{
	const char *str = tk->tk_str;
	size_t len = tk->tk_len;

	return len >= 2 && str[len - 1] == '\n' && str[len - 2] == '\n';
}

static void
token_trim_verbatim_line(struct token *tk)
{
	if (token_has_verbatim_line(tk))
		tk->tk_len--;
}
