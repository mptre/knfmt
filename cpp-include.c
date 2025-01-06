#include "cpp-include.h"

#include "config.h"

#include <err.h>
#include <string.h>

#include "libks/arena.h"
#include "libks/map.h"
#include "libks/string.h"
#include "libks/vector.h"

#include "lexer.h"
#include "options.h"
#include "simple.h"
#include "style.h"
#include "token.h"

struct cpp_include {
	VECTOR(struct include)		 includes;
	MAP(int,, struct include_group)	 groups;
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
	struct include_priority	 priority;
};

struct include_group {
	VECTOR(struct include *)	includes;
};

static void	cpp_include_free(void *);
static void	cpp_include_exec(struct cpp_include *, struct lexer *);
static void	cpp_include_reset(struct cpp_include *);

static struct token	*add_line(struct cpp_include *, struct lexer *,
    struct token *);

static const char	*findpath(const char *, size_t, struct arena_scope *);

static void	token_trim_verbatim_line(struct token *);

struct cpp_include *
cpp_include_alloc(const struct style *st, struct simple *si,
    struct token_list *prefixes, struct arena *scratch,
    const struct options *op, struct arena_scope *eternal_scope)
{
	struct cpp_include *ci;
	VECTOR(int) priorities;
	size_t i;
	unsigned int simple_flags;

	ci = arena_calloc(eternal_scope, 1, sizeof(*ci));
	arena_cleanup(eternal_scope, cpp_include_free, ci);
	if (VECTOR_INIT(ci->includes))
		err(1, NULL);
	if (MAP_INIT(ci->groups))
		err(1, NULL);
	ci->prefixes = prefixes;
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

	simple_flags = style(st, SortIncludes) == CaseSensitive ?
	    SIMPLE_FORCE : 0;
	simple_enter(si, SIMPLE_CPP_SORT_INCLUDES, simple_flags, &ci->cookie);

	return ci;
}

static void
cpp_include_free(void *arg)
{
	struct cpp_include *ci = arg;
	MAP_ITERATOR(ci->groups) it = {0};

	cpp_include_reset(ci);
	VECTOR_FREE(ci->includes);
	while (MAP_ITERATE(ci->groups, &it)) {
		struct include_group *group = it.val;

		VECTOR_FREE(group->includes);
		MAP_REMOVE(ci->groups, it.key);
	}
	MAP_FREE(ci->groups);
}

void
cpp_include_done(struct cpp_include *ci)
{
	simple_leave(&ci->cookie);
}

void
cpp_include_leave(struct cpp_include *ci, struct lexer *lx)
{
	if (!is_simple_enabled(ci->si, SIMPLE_CPP_SORT_INCLUDES))
		return;

	cpp_include_exec(ci, lx);
	cpp_include_reset(ci);
}

void
cpp_include_add(struct cpp_include *ci, struct lexer *lx, struct token *tk)
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

	cpp_include_exec(ci, lx);
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

static char **
split_on_period(const char *str, struct arena_scope *s)
{
	static struct KS_str_match match;
	KS_str_match_init_once("..", &match);
	return KS_str_split(str, &match, s);
}

static char **
split_on_slash(const char *str, struct arena_scope *s)
{
	static struct KS_str_match match;
	KS_str_match_init_once("//", &match);
	return KS_str_split(str, &match, s);
}

static int
is_main_include(const char *include_path, const char *path,
    struct arena *scratch)
{
	const char *basename, *filename, *include_main;
	char **str;

	arena_scope(scratch, s);

	/* Transform path "a/b.c" into "a/b.h". */
	str = VECTOR_FIRST(split_on_period(path, &s));
	if (str == NULL)
		return 0; /* UNREACHABLE */
	basename = *str;
	include_main = arena_sprintf(&s, "\"%s.h\"", basename);
	if (strcmp(include_path, include_main) == 0)
		return 1;

	/* Transform path "a/b.c" into "b.h". */
	str = VECTOR_LAST(split_on_slash(path, &s));
	if (str == NULL)
		return 0; /* UNREACHABLE */
	filename = *str;
	str = VECTOR_FIRST(split_on_period(filename, &s));
	if (str == NULL)
		return 0; /* UNREACHABLE */
	basename = *str;
	include_main = arena_sprintf(&s, "\"%s.h\"", basename);
	if (strcmp(include_path, include_main) == 0)
		return 1;

	return 0;
}

static void
cpp_include_exec(struct cpp_include *ci, struct lexer *lx)
{
	MAP_ITERATOR(ci->groups) it = {0};
	struct token *after = ci->after;
	const char *path = lexer_get_path(lx);
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
		const char *include_path;

		include_path = findpath(tk->tk_str, tk->tk_len, &s);
		if (include_path == NULL)
			return;

		if (ci->regroup) {
			if (!is_main_include(include_path, path, ci->scratch)) {
				priority = style_include_priority(ci->st,
				    include_path);
			}
		} else {
			if (include_path[0] == '<')
				nbrackets++;
			if (include_path[0] == '"')
				nquotes++;
			if (strchr(include_path, '/') != NULL)
				nslashes++;
			if ((nbrackets > 0 && nquotes > 0) ||
			    (nbrackets > 0 && nslashes > 0))
				return;

			/* Allow the main include to come first. */
			if (i == 0 &&
			    is_main_include(include_path, path, ci->scratch))
				priority.sort = -1;
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
			after = add_line(ci, lx, after);
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
add_line(struct cpp_include *ci, struct lexer *lx, struct token *after)
{
	struct token *tk;

	tk = lexer_emit_synthetic(lx, &(struct token){
	    .tk_type	= TOKEN_SPACE,
	    .tk_str	= "\n",
	    .tk_len	= 1,
	});
	token_list_append_after(ci->prefixes, after, tk);
	return tk;
}

static const char *
findpath(const char *str, size_t len, struct arena_scope *s)
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
	return arena_strndup(s, so, (size_t)(eo - so) + 1);
}

static void
token_trim_verbatim_line(struct token *tk)
{
	if (token_has_verbatim_line(tk, 2))
		tk->tk_len--;
}
