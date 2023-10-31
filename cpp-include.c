#include "cpp-include.h"

#include "config.h"

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "libks/arena.h"
#include "libks/map.h"
#include "libks/vector.h"

#include "alloc.h"
#include "lexer.h"
#include "options.h"
#include "simple.h"
#include "style.h"
#include "token.h"

struct cpp_include {
	VECTOR(struct include)		 includes;
	MAP(int,, struct include_group)	 groups;
	struct lexer			*lx;
	struct token_list		*prefixes;
	struct token			*after;
	const struct style		*st;
	struct simple			*si;
	struct simple_cookie		 cookie;
	struct arena			*scratch;
	const struct options		*op;
	int				 ignore;
	int				 regroup;
};

struct include {
	struct token		*tk;
	struct {
		const char	*str;
		size_t		 len;
	} path;
	struct include_priority	 priority;
};

struct include_group {
	VECTOR(struct include *)	includes;
};

static void	cpp_include_exec(struct cpp_include *);
static void	cpp_include_reset(struct cpp_include *);

static struct token	*add_line(struct cpp_include *, struct token *);

static const char	*findpath(const char *, size_t, size_t *);

static void	token_trim_verbatim_line(struct token *);

struct cpp_include *
cpp_include_alloc(const struct style *st, struct simple *si,
    struct arena *scratch, const struct options *op)
{
	struct cpp_include *ci;
	VECTOR(int) priorities;
	size_t i;

	ci = ecalloc(1, sizeof(*ci));
	if (VECTOR_INIT(ci->includes))
		err(1, NULL);
	if (MAP_INIT(ci->groups))
		err(1, NULL);
	ci->st = st;
	ci->si = si;
	ci->scratch = scratch;
	ci->op = op;
	ci->regroup = style(st, IncludeBlocks) == Regroup;

	priorities = style_include_priorities(st);
	for (i = 0; i < VECTOR_LENGTH(priorities); i++) {
		struct include_group *group;

		if (MAP_FIND(ci->groups, priorities[i]) != NULL)
			continue;

		group = MAP_INSERT(ci->groups, priorities[i]);
		if (group == NULL)
			err(1, NULL);
		if (VECTOR_INIT(group->includes))
			err(1, NULL);
	}
	VECTOR_FREE(priorities);

	return ci;
}

void
cpp_include_free(struct cpp_include *ci)
{
	MAP_ITERATOR(ci->groups) it = {0};

	if (ci == NULL)
		return;

	cpp_include_reset(ci);
	VECTOR_FREE(ci->includes);
	while (MAP_ITERATE(ci->groups, &it)) {
		struct include_group *group = it.val;

		VECTOR_FREE(group->includes);
		MAP_REMOVE(ci->groups, it.key);
	}
	MAP_FREE(ci->groups);
	free(ci);
}

void
cpp_include_enter(struct cpp_include *ci, struct lexer *lx,
    struct token_list *prefixes)
{
	unsigned int simple_flags;

	simple_flags = style(ci->st, SortIncludes) == CaseSensitive ?
	    SIMPLE_FORCE : 0;
	if (!simple_enter(ci->si, SIMPLE_CPP_SORT_INCLUDES, simple_flags,
	    &ci->cookie))
		return;

	assert(ci->lx == NULL);
	ci->lx = lx;
	assert(ci->prefixes == NULL);
	ci->prefixes = prefixes;
}

void
cpp_include_leave(struct cpp_include *ci)
{
	if (!is_simple_enabled(ci->si, SIMPLE_CPP_SORT_INCLUDES))
		return;

	cpp_include_exec(ci);
	cpp_include_reset(ci);
	ci->lx = NULL;
	ci->prefixes = NULL;

	simple_leave(&ci->cookie);
}

void
cpp_include_add(struct cpp_include *ci, struct token *tk)
{
	if (!is_simple_enabled(ci->si, SIMPLE_CPP_SORT_INCLUDES))
		return;

	if (tk->tk_type == TOKEN_CPP_INCLUDE) {
		struct include *include;

		if (ci->op->diffparse &&
		    (tk->tk_flags & TOKEN_FLAG_DIFF) == 0)
			ci->ignore = 1;

		if (VECTOR_EMPTY(ci->includes))
			ci->after = token_prev(tk);
		include = VECTOR_ALLOC(ci->includes);
		if (include == NULL)
			err(1, NULL);
		token_ref(tk);
		include->tk = tk;
		if (ci->regroup || !token_has_verbatim_line(tk, 2))
			return;
	}

	cpp_include_exec(ci);
	cpp_include_reset(ci);
}

static void
add_to_include_group(struct cpp_include *ci, struct include *include)
{
	struct include_group *group;
	struct include **dst;

	group = MAP_FIND(ci->groups, include->priority.group);
	dst = VECTOR_ALLOC(group->includes);
	if (dst == NULL)
		err(1, NULL);
	*dst = include;
}

static int
include_cmp(struct include *const *aa, struct include *const *bb)
{
	const struct include *a = *aa;
	const struct include *b = *bb;

	if (a->priority.sort < b->priority.sort)
		return -1;
	if (a->priority.sort > b->priority.sort)
		return 1;
	return token_strcmp(a->tk, b->tk);
}

static void
cpp_include_exec(struct cpp_include *ci)
{
	MAP_ITERATOR(ci->groups) it = {0};
	struct token *after = ci->after;
	size_t i, nincludes;
	unsigned int nbrackets = 0;
	unsigned int nquotes = 0;
	unsigned int nslashes = 0;

	if (ci->ignore)
		return;

	nincludes = VECTOR_LENGTH(ci->includes);
	if (nincludes < 2)
		return;

	arena_scope(ci->scratch, s);

	for (i = 0; i < nincludes; i++) {
		struct include_priority priority = {0};
		struct include *include = &ci->includes[i];
		const struct token *tk = include->tk;
		const char *path;
		size_t len;

		path = findpath(tk->tk_str, tk->tk_len, &len);
		if (path == NULL)
			return;
		include->path.str = path;
		include->path.len = len;

		if (ci->regroup) {
			const char *str;

			str = arena_strndup(&s, path, len);
			priority = style_include_priority(ci->st, str,
			    lexer_get_path(ci->lx));
		} else {
			if (path[0] == '<')
				nbrackets++;
			if (path[0] == '"')
				nquotes++;
			if (memchr(path, '/', len) != NULL)
				nslashes++;
			if ((nbrackets > 0 && nquotes > 0) ||
			    (nbrackets > 0 && nslashes > 0))
				return;
		}
		include->priority = priority;
		add_to_include_group(ci, include);
	}

	while (MAP_ITERATE(ci->groups, &it)) {
		struct include_group *group = it.val;
		struct include **p;
		struct include *last;
		int doline;

		p = VECTOR_LAST(group->includes);
		if (p == NULL)
			continue;
		last = *p;
		doline = ci->regroup || token_has_verbatim_line(last->tk, 2);

		VECTOR_SORT(group->includes, include_cmp);

		nincludes = VECTOR_LENGTH(group->includes);
		for (i = 0; i < nincludes; i++) {
			struct include *include = group->includes[i];

			token_trim_verbatim_line(include->tk);
			token_list_remove(ci->prefixes, include->tk);
			token_ref(include->tk);
			if (after != NULL) {
				token_list_append_after(ci->prefixes, after,
				    include->tk);
			} else {
				token_list_prepend(ci->prefixes, include->tk);
			}
			after = include->tk;
		}
		if (doline)
			after = add_line(ci, after);
	}
}

static void
cpp_include_reset(struct cpp_include *ci)
{
	MAP_ITERATOR(ci->groups) it = {0};

	while (!VECTOR_EMPTY(ci->includes)) {
		struct include *include;

		include = VECTOR_POP(ci->includes);
		token_rele(include->tk);
	}
	while (MAP_ITERATE(ci->groups, &it)) {
		struct include_group *group = it.val;

		VECTOR_CLEAR(group->includes);
	}
	ci->after = NULL;
	ci->ignore = 0;
}

static struct token *
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
	token_list_append_after(ci->prefixes, after, tk);
	return tk;
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

static void
token_trim_verbatim_line(struct token *tk)
{
	if (token_has_verbatim_line(tk, 2))
		tk->tk_len--;
}
