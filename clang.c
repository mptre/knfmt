#include "clang.h"

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <string.h>

#include "libks/arena-buffer.h"
#include "libks/arena.h"
#include "libks/buffer.h"
#include "libks/vector.h"

#include "comment.h"
#include "cpp-align.h"
#include "cpp-include-guard.h"
#include "cpp-include.h"
#include "lexer-callbacks.h"
#include "lexer.h"
#include "token.h"
#include "trace.h"

#ifdef HAVE_QUEUE
#  include <sys/queue.h>
#else
#  include "compat-queue.h"
#endif

#define clang_trace(cl, fmt, ...) trace('c', (cl)->op, (fmt), __VA_ARGS__)

struct clang {
	const struct style	*st;
	const struct options	*op;
	struct cpp_include	*ci;
	struct token_list	 prefixes;
	VECTOR(struct token *)	 branches;
	VECTOR(struct token *)	 stamps;

	struct {
		struct arena_scope	*eternal_scope;
		struct arena		*scratch;
	} arena;
};

static void	clang_branch_enter(struct clang *, struct lexer *,
    struct token *,
    struct token *);
static void	clang_branch_link(struct clang *, struct lexer *,
    struct token *,
    struct token *);
static void	clang_branch_leave(struct clang *, struct lexer *,
    struct token *,
    struct token *);
static void	clang_branch_purge(struct clang *, struct lexer *);

static void		 clang_free(void *);
static void		 clang_after_read(struct lexer *, void *);
static void		 clang_before_free(struct lexer *, void *);
static struct token	*clang_read(struct lexer *, void *);
static struct token	*clang_read_prefix(struct clang *, struct lexer *);
static struct token	*clang_read_comment(struct clang *, struct lexer *,
    int);
static struct token	*clang_read_cpp(struct clang *, struct lexer *);
static struct token	*clang_keyword(struct lexer *);
static void		 clang_insert_keyword(const struct token *);
static struct token	*clang_find_keyword(const struct lexer *,
    const struct lexer_state *);
static struct token	*clang_find_keyword1(const char *, size_t);
static struct token	*clang_find_alias(struct lexer *,
    const struct lexer_state *);
static struct token	*clang_ellipsis(struct lexer *,
    const struct lexer_state *);
static struct token	*clang_token_alloc(struct arena_scope *,
    const struct token *);
static const char	*clang_token_serialize(const struct token *,
    struct arena_scope *);
static const char	*clang_token_serialize_prefix(const struct token *,
    struct arena_scope *);

static struct token	*clang_end_of_branch(struct token *);

static struct token	*clang_last_stamped(struct clang *);
static struct token	*clang_recover_find_branch(struct token *,
    struct token *, int);
static void		 clang_branch_fold(struct clang *, struct lexer *,
    struct token *, struct token **);

static void		 remove_token(struct lexer *, struct token *);

static void	token_branch_link(struct token *, struct token *);
static void	token_branch_revert(struct token *);
static void	token_prolong(struct token *, struct token *);

static int	isnum(unsigned char);

static struct token *table_tokens[256];

static const struct token tklit = {
	.tk_type	= TOKEN_LITERAL,
};
static const struct token tkstr = {
	.tk_type	= TOKEN_STRING,
};

void
clang_init(void)
{
#define OP(type, keyword, flags) {					\
	.tk_type	= (type),					\
	.tk_flags	= (flags),					\
	.tk_str		= (keyword),					\
	.tk_len		= sizeof((keyword)) - 1,			\
	},
	static struct token keywords[] = { FOR_TOKEN_TYPES(OP) };
	static struct token aliases[] = { FOR_TOKEN_ALIASES(OP) };
#undef OP
	const struct token *token_types[TOKEN_NONE + 1] = {0};
	size_t i;

	for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
		const struct token *src = &keywords[i];

		clang_insert_keyword(src);
		assert(token_types[src->tk_type] == NULL);
		token_types[src->tk_type] = src;
	}

	/* Let aliases inherit token flags. */
	for (i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
		struct token *src = &aliases[i];

		src->tk_flags = token_types[src->tk_type]->tk_flags;
		clang_insert_keyword(src);
	}
}

void
clang_shutdown(void)
{
	size_t nslots = sizeof(table_tokens) / sizeof(table_tokens[0]);
	size_t i;

	for (i = 0; i < nslots; i++)
		VECTOR_FREE(table_tokens[i]);
}

struct clang *
clang_alloc(const struct style *st, struct simple *si,
    struct arena_scope *eternal_scope, struct arena *scratch,
    const struct options *op)
{
	struct clang *cl;

	cl = arena_calloc(eternal_scope, 1, sizeof(*cl));
	arena_cleanup(eternal_scope, clang_free, cl);
	TAILQ_INIT(&cl->prefixes);
	cl->st = st;
	cl->op = op;
	cl->arena.eternal_scope = eternal_scope;
	cl->arena.scratch = scratch;
	cl->ci = cpp_include_alloc(st, si, &cl->prefixes, eternal_scope,
	    scratch, op);
	if (VECTOR_INIT(cl->branches))
		err(1, NULL);
	if (VECTOR_INIT(cl->stamps))
		err(1, NULL);
	return cl;
}

static void
clang_free(void *arg)
{
	struct clang *cl = arg;

	VECTOR_FREE(cl->branches);
	VECTOR_FREE(cl->stamps);
}

struct lexer_callbacks
clang_lexer_callbacks(struct clang *cl)
{
	return (struct lexer_callbacks){
	    .read		= clang_read,
	    .alloc		= clang_token_alloc,
	    .serialize		= clang_token_serialize,
	    .serialize_prefix	= clang_token_serialize_prefix,
	    .end_of_branch	= clang_end_of_branch,
	    .after_read		= clang_after_read,
	    .before_free	= clang_before_free,
	    .arg		= cl,
	};
}

/*
 * Take note of the last consumed token, later used while branching and
 * recovering.
 */
void
clang_stamp(struct clang *cl, struct lexer *lx)
{
	struct token **dst;
	struct token *back;

	if (!lexer_back(lx, &back))
		return;
	clang_trace(cl, "stamp %s", lexer_serialize(lx, back));
	token_ref(back);
	dst = VECTOR_ALLOC(cl->stamps);
	if (dst == NULL)
		err(1, NULL);
	*dst = back;
}

/*
 * Returns non-zero if the lexer took the next branch.
 */
int
clang_branch(struct clang *cl, struct lexer *lx, struct token **unmute)
{
	struct token *back, *cpp_dst, *cpp_src, *dst, *rm, *seek, *src;
	int error = 0;

	if (!lexer_back(lx, &back))
		return 0;
	clang_trace(cl, "back %s", lexer_serialize(lx, back));
	cpp_src = token_get_branch(back);
	if (cpp_src == NULL)
		return 0;
	token_ref(cpp_src);

	src = cpp_src->tk_branch.br_parent;
	token_ref(src);
	cpp_dst = cpp_src->tk_branch.br_nx;
	token_ref(cpp_dst);
	dst = cpp_dst->tk_branch.br_parent;
	token_ref(dst);

	clang_trace(cl, "branch from %s to %s, covering [%s, %s)",
	    lexer_serialize(lx, cpp_src), lexer_serialize(lx, cpp_dst),
	    lexer_serialize(lx, src), lexer_serialize(lx, dst));

	token_branch_unlink(cpp_src);

	rm = src;
	for (;;) {
		struct token *nx;

		clang_trace(cl, "removing %s", lexer_serialize(lx, rm));

		nx = token_next(rm);
		remove_token(lx, rm);
		if (nx == dst)
			break;
		rm = nx;
	}

	/*
	 * Instruct caller that crossing this token must cause tokens to be
	 * emitted again.
	 */
	*unmute = dst;

	/* Rewind to last stamped token. */
	seek = clang_last_stamped(cl);
	if (seek != NULL)
		lexer_seek_after(lx, seek);
	else if (lexer_peek_first(lx, &seek))
		lexer_seek(lx, seek);
	else
		error = 1;

	token_rele(dst);
	token_rele(cpp_dst);
	token_rele(src);
	token_rele(cpp_src);

	return error ? 0 : 1;
}

/*
 * Try to recover after encountering invalid source code. Returns the index of
 * the stamped token seeked to, starting from the end. This index should
 * correspond to the number of documents that must be removed since we're about
 * to parse them again.
 */
int
clang_recover(struct clang *cl, struct lexer *lx, struct token **unmute)
{
	struct token *seek = NULL;
	struct token *back, *br, *dst, *src, *stamp;
	size_t i;
	int error = 0;
	int ndocs = 1;

	if (!lexer_back(lx, &back) && !lexer_peek_first(lx, &back))
		return 0;
	stamp = clang_last_stamped(cl);
	clang_trace(cl, "back %s, stamp %s",
	    lexer_serialize(lx, back), lexer_serialize(lx, stamp));
	br = clang_recover_find_branch(back, stamp, 0);
	if (br == NULL)
		br = clang_recover_find_branch(back, stamp, 1);
	if (br == NULL)
		return 0;

	src = br->tk_branch.br_parent;
	dst = br->tk_branch.br_nx->tk_branch.br_parent;
	clang_trace(cl, "branch from %s to %s covering [%s, %s)",
	    lexer_serialize(lx, br),
	    lexer_serialize(lx, br->tk_branch.br_nx),
	    lexer_serialize(lx, src),
	    lexer_serialize(lx, dst));

	/*
	 * Find the offset of the first stamped token before the branch.
	 * Must be done before getting rid of the branch as stamped tokens might
	 * be removed.
	 */
	for (i = VECTOR_LENGTH(cl->stamps); i > 0; i--) {
		stamp = cl->stamps[i - 1];
		if (!token_is_dangling(stamp) && token_cmp(stamp, br) < 0)
			break;
		ndocs++;
	}
	clang_trace(cl, "removing %d document(s)", ndocs);

	/*
	 * Turn the whole branch into a prefix. As the branch is about to be
	 * removed, grab a reference since it's needed below.
	 */
	token_ref(br);
	clang_branch_fold(cl, lx, br, unmute);

	/* Find first stamped token before the branch. */
	for (i = VECTOR_LENGTH(cl->stamps); i > 0; i--) {
		stamp = cl->stamps[i - 1];
		if (!token_is_dangling(stamp) && token_cmp(stamp, br) < 0) {
			seek = stamp;
			break;
		}
	}
	token_rele(br);

	if (seek != NULL)
		lexer_seek_after(lx, seek);
	else if (lexer_peek_first(lx, &seek))
		lexer_seek(lx, seek);
	else
		error = 1;

	return error ? 0 : ndocs;
}

void
clang_token_branch_unlink(struct token *tk)
{
	token_branch_unlink(tk);
}

static void
clang_after_read(struct lexer *lx, void *arg)
{
	struct clang *cl = arg;

	clang_branch_purge(cl, lx);
	cpp_include_done(cl->ci);
	cpp_include_guard(cl->st, lx, cl->arena.eternal_scope,
	    cl->arena.scratch);
}

static void
clang_before_free(struct lexer *lx, void *arg)
{
	struct clang *cl = arg;
	struct token *tk;

	/* Must unlink all branches to drop references to parent tokens. */
	if (lexer_peek_first(lx, &tk)) {
		for (; tk != NULL; tk = token_next(tk)) {
			struct token *prefix;

			TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry)
				token_branch_unlink(prefix);
		}
	}

	while (!VECTOR_EMPTY(cl->stamps)) {
		struct token **tail;

		tail = VECTOR_POP(cl->stamps);
		token_rele(*tail);
	}
}

static struct token *
clang_read(struct lexer *lx, void *arg)
{
	struct clang *cl = arg;
	struct token *prefix, *t, *tk, *tmp;
	struct lexer_state st;
	int ncomments = 0;
	int nlines;
	unsigned char ch;

	/* Consume all comments and preprocessor directives. */
	for (;;) {
		prefix = clang_read_prefix(cl, lx);
		if (prefix == NULL)
			break;
		cpp_include_add(cl->ci, lx, prefix);
	}
	cpp_include_leave(cl->ci, lx);

	tk = clang_keyword(lx);
	if (tk != NULL)
		goto out;

	st = lexer_get_state(lx);
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
	} else if (isdigit(ch) || ch == '.') {
		do {
			if (lexer_getc(lx, &ch))
				goto eof;
		} while (isnum(ch));
		lexer_ungetc(lx);
		tk = lexer_emit(lx, &st, &tklit);
	} else if (isalpha(ch) || ch == '_') {
		lexer_match(lx, "AZaz09__");

		if ((t = clang_find_keyword(lx, &st)) != NULL) {
			tk = lexer_emit(lx, &st, t);
		} else if ((tk = clang_find_alias(lx, &st)) != NULL) {
			/* nothing */
		} else {
			/* Fallback, treat everything as an identifier. */
			tk = lexer_emit(lx, &st, &(struct token){
			    .tk_type	= TOKEN_IDENT,
			});
		}
	} else if (lexer_eof(lx)) {
eof:
		tk = lexer_emit(lx, &st, &(struct token){
		    .tk_type	= LEXER_EOF,
		    .tk_str	= "",
		});
	} else {
		tk = lexer_emit(lx, &st, &(struct token){
		    .tk_type	= TOKEN_NONE,
		});
	}

out:
	TAILQ_CONCAT(&tk->tk_prefixes, &cl->prefixes, tk_entry);

	/*
	 * Consume trailing/interwined comments. If the token is about to be
	 * discarded, skip this causing any comment to be treated as a prefix
	 * instead.
	 */
	if ((tk->tk_flags & TOKEN_FLAG_DISCARD) == 0) {
		for (;;) {
			struct token *comment;

			comment = clang_read_comment(cl, lx, 0);
			if (comment == NULL)
				break;
			token_list_append(&tk->tk_suffixes, comment);
			ncomments++;
		}
	}

	/*
	 * Trailing whitespace is only honored if it's present immediately after
	 * the token.
	 */
	if (ncomments == 0 && lexer_eat_spaces(lx, &tmp)) {
		tmp->tk_flags |= TOKEN_FLAG_OPTSPACE | TOKEN_FLAG_DISCARD;
		token_list_append(&tk->tk_suffixes, tmp);
	}

	/* Consume hard line(s). */
	nlines = lexer_eat_lines(lx, 0, &tmp);
	if (nlines > 0) {
		if (nlines == 1)
			tmp->tk_flags |= TOKEN_FLAG_OPTLINE;
		token_list_append(&tk->tk_suffixes, tmp);
	}

	/* Establish links between cpp branches. */
	TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
		switch (prefix->tk_type) {
		case TOKEN_CPP_IF:
		case TOKEN_CPP_IFNDEF:
			clang_branch_enter(cl, lx, prefix, tk);
			break;
		case TOKEN_CPP_ELSE:
			clang_branch_link(cl, lx, prefix, tk);
			break;
		case TOKEN_CPP_ENDIF:
			clang_branch_leave(cl, lx, prefix, tk);
			break;
		}
	}

	return tk;
}

static struct token *
clang_token_alloc(struct arena_scope *s, const struct token *def)
{
	return token_alloc(s, 0, def);
}

static const char *
clang_token_serialize(const struct token *tk, struct arena_scope *s)
{
	return token_serialize(s, tk,
	    TOKEN_SERIALIZE_POSITION | TOKEN_SERIALIZE_FLAGS);
}

static const char *
clang_token_serialize_prefix(const struct token *prefix, struct arena_scope *s)
{
	struct buffer *bf;

	bf = arena_buffer_alloc(s, 1 << 8);
	buffer_printf(bf, "%s", clang_token_serialize(prefix, s));
	if (prefix->tk_branch.br_pv != NULL) {
		buffer_printf(bf, ", pv %s",
		    clang_token_serialize(prefix->tk_branch.br_pv, s));
	}
	if (prefix->tk_branch.br_nx != NULL) {
		buffer_printf(bf, ", nx %s",
		    clang_token_serialize(prefix->tk_branch.br_nx, s));
	}
	return buffer_str(bf);
}

static struct token *
clang_end_of_branch(struct token *tk)
{
	struct token *br;

	for (br = token_get_branch(tk); br->tk_branch.br_nx != NULL;
	    br = br->tk_branch.br_nx)
		continue;
	return br->tk_branch.br_parent;
}

static struct token *
clang_last_stamped(struct clang *cl)
{
	size_t i;

	for (i = VECTOR_LENGTH(cl->stamps); i > 0; i--) {
		struct token *stamp = cl->stamps[i - 1];

		if (!token_is_dangling(stamp))
			return stamp;
	}
	return NULL;
}

/*
 * Find the best suited branch to fold relative to the given token while trying
 * to recover after encountering invalid source code.
 */
static struct token *
clang_recover_find_branch(struct token *tk, struct token *threshold,
    int forward)
{
	for (;;) {
		struct token *prefix;

		TAILQ_FOREACH_REVERSE(prefix, &tk->tk_prefixes, token_list,
		    tk_entry) {
			struct token *br, *nx;
			int token_type;

			token_type = token_type_normalize(prefix);
			if (token_type == TOKEN_CPP_IF)
				br = prefix;
			else if (token_type == TOKEN_CPP_ELSE)
				br = prefix;
			else if (token_type == TOKEN_CPP_ENDIF)
				br = prefix->tk_branch.br_pv;
			else
				continue;

			nx = br->tk_branch.br_nx;
			if (br->tk_branch.br_parent == nx->tk_branch.br_parent)
				continue;
			return br;
		}

		if (forward)
			tk = token_next(tk);
		else
			tk = token_prev(tk);
		if (tk == NULL || tk == threshold)
			break;
	}

	return NULL;
}

/*
 * Fold tokens covered by the branch into a prefix.
 */
static void
clang_branch_fold(struct clang *cl, struct lexer *lx, struct token *cpp_src,
    struct token **unmute)
{
	const struct lexer_state st = {
		.st_lno	= cpp_src->tk_lno,
		.st_off	= cpp_src->tk_off,
	};
	struct token *cpp_dst, *dst, *prefix, *pv, *rm, *src;
	size_t len;
	int dangling = 0;

	src = cpp_src->tk_branch.br_parent;
	token_ref(src);

	cpp_dst = cpp_src->tk_branch.br_nx;
	token_ref(cpp_dst);
	dst = cpp_dst->tk_branch.br_parent;
	token_ref(dst);

	len = (cpp_dst->tk_off + cpp_dst->tk_len) - cpp_src->tk_off;
	prefix = lexer_emit(lx, &st, &(struct token){
	    .tk_type	= TOKEN_CPP,
	    .tk_flags	= TOKEN_FLAG_CPP,
	    .tk_str	= cpp_src->tk_str,
	    .tk_len	= len,
	});

	/*
	 * Remove all prefixes hanging of the destination covered by the new
	 * prefix token.
	 */
	while (!TAILQ_EMPTY(&dst->tk_prefixes)) {
		struct token *pr;

		pr = token_list_first(&dst->tk_prefixes);
		clang_trace(cl, "removing prefix %s", lexer_serialize(lx, pr));
		token_branch_unlink(pr);
		token_list_remove(&dst->tk_prefixes, pr);
		if (pr == cpp_dst)
			break;
	}

	clang_trace(cl, "add prefix %s to %s",
	    lexer_serialize(lx, prefix),
	    lexer_serialize(lx, dst));
	TAILQ_INSERT_HEAD(&dst->tk_prefixes, prefix, tk_entry);

	/*
	 * Keep any existing prefix not covered by the new prefix token
	 * by moving them to the destination.
	 */
	pv = token_prev(cpp_src);
	for (;;) {
		struct token *tmp;

		if (pv == NULL)
			break;

		clang_trace(cl, "keeping prefix %s", lexer_serialize(lx, pv));
		tmp = token_prev(pv);
		token_move_prefix(pv, src, dst);
		pv = tmp;
	}

	/*
	 * Remove all tokens up to the destination covered by the new prefix
	 * token. Some tokens might already have been removed by an overlapping
	 * branch, therefore abort while encountering a dangling token.
	 */
	rm = src;
	while (!dangling) {
		struct token *nx;

		if (rm == dst)
			break;

		dangling = token_is_dangling(rm);
		nx = token_next(rm);
		clang_trace(cl, "removing %s", lexer_serialize(lx, rm));
		remove_token(lx, rm);
		rm = nx;
	}

	/*
	 * Instruct caller that crossing this token must cause tokens to be
	 * emitted again.
	 */
	*unmute = dst;

	token_rele(dst);
	token_rele(cpp_dst);
	token_rele(src);
}

static void
remove_token(struct lexer *lx, struct token *tk)
{
	while (!TAILQ_EMPTY(&tk->tk_prefixes)) {
		struct token *prefix = TAILQ_FIRST(&tk->tk_prefixes);

		token_branch_unlink(prefix);
		token_list_remove(&tk->tk_prefixes, prefix);
	}

	while (!TAILQ_EMPTY(&tk->tk_suffixes)) {
		struct token *suffix = TAILQ_FIRST(&tk->tk_suffixes);

		token_list_remove(&tk->tk_suffixes, suffix);
	}

	lexer_remove(lx, tk);
}

static void
clang_branch_enter(struct clang *cl, struct lexer *lx, struct token *cpp,
    struct token *parent)
{
	struct token **br;

	clang_trace(cl, "%s", lexer_serialize(lx, cpp));
	token_branch_parent(cpp, parent);
	br = VECTOR_ALLOC(cl->branches);
	if (br == NULL)
		err(1, NULL);
	*br = cpp;
}

static void
clang_branch_link(struct clang *cl, struct lexer *lx, struct token *cpp,
    struct token *parent)
{
	struct token **last;
	struct token *br;

	/* Silently ignore broken branch. */
	last = VECTOR_LAST(cl->branches);
	if (last == NULL) {
		token_branch_revert(cpp);
		return;
	}
	br = *last;

	clang_trace(cl, "%s -> %s",
	    lexer_serialize(lx, br), lexer_serialize(lx, cpp));

	token_branch_parent(cpp, parent);
	token_branch_link(br, cpp);
	*last = cpp;
}

static void
clang_branch_leave(struct clang *cl, struct lexer *lx, struct token *cpp,
    struct token *parent)
{
	struct token **last;

	clang_branch_link(cl, lx, cpp, parent);

	last = VECTOR_LAST(cl->branches);
	if (last != NULL) {
		struct token *br;

		for (br = *last; br != NULL; br = br->tk_branch.br_pv) {
			struct token *p = br->tk_branch.br_parent;

			if (token_is_branch(p))
				p->tk_flags |= TOKEN_FLAG_BRANCH;
		}
	}

	VECTOR_POP(cl->branches);
}

/*
 * Purge pending broken branches.
 */
static void
clang_branch_purge(struct clang *cl, struct lexer *lx)
{
	while (!VECTOR_EMPTY(cl->branches)) {
		struct token **tail;
		struct token *pv, *tk;

		tail = VECTOR_POP(cl->branches);
		tk = *tail;
		do {
			pv = tk->tk_branch.br_pv;
			clang_trace(cl, "broken branch: %s%s%s",
			    lexer_serialize(lx, tk),
			    pv ? " -> " : "",
			    pv ? lexer_serialize(lx, pv) : "");
			token_branch_revert(tk);
			tk = pv;
		} while (tk != NULL);
	}
}

static struct token *
clang_read_prefix(struct clang *cl, struct lexer *lx)
{
	struct token *comment, *cpp;

	comment = clang_read_comment(cl, lx, 1);
	if (comment != NULL) {
		struct token *pv;

		pv = TAILQ_LAST(&cl->prefixes, token_list);
		if (pv != NULL &&
		    pv->tk_type == TOKEN_COMMENT &&
		    token_cmp(comment, pv) == 0) {
			token_prolong(pv, comment);
			return pv;
		}
		token_list_append(&cl->prefixes, comment);
		return comment;
	}

	cpp = clang_read_cpp(cl, lx);
	if (cpp != NULL) {
		token_list_append(&cl->prefixes, cpp);
		return cpp;
	}

	return NULL;
}

static int
peek_c99_comment(struct lexer *lx)
{
	struct lexer_state st;
	int peek = 0;
	unsigned char ch;

	st = lexer_get_state(lx);
	lexer_eat_spaces(lx, NULL);
	if (lexer_getc(lx, &ch) == 0 && ch == '/' &&
	    lexer_getc(lx, &ch) == 0 && ch == '/')
		peek = 1;
	lexer_set_state(lx, &st);
	return peek;
}

static struct token *
clang_read_comment(struct clang *cl, struct lexer *lx, int block)
{
	struct lexer_state oldst, st;
	struct buffer *bf;
	struct token *tk;
	int c99;
	unsigned char ch;

	oldst = st = lexer_get_state(lx);
again:
	if (block)
		lexer_eat_lines_and_spaces(lx, &st);
	else
		lexer_eat_spaces(lx, NULL);
	if (lexer_getc(lx, &ch) || ch != '/') {
		lexer_set_state(lx, &oldst);
		return NULL;
	}
	if (lexer_getc(lx, &ch) || (ch != '/' && ch != '*')) {
		lexer_set_state(lx, &oldst);
		return NULL;
	}

	c99 = ch == '/';

	if (c99) {
		for (;;) {
			if (lexer_getc(lx, &ch))
				break;
			if (ch == '\n') {
				if (peek_c99_comment(lx))
					goto again;
				lexer_ungetc(lx);
				break;
			}
		}
	} else {
		unsigned char peek;

		ch = '\0';
		for (;;) {
			if (lexer_getc(lx, &peek))
				break;
			if (ch == '*' && peek == '/')
				break;
			ch = peek;
		}
	}

	if (block) {
		/*
		 * For block comments, consume trailing whitespace and up to 2
		 * hard lines(s), will be hanging of the comment token.
		 */
		lexer_eat_spaces(lx, NULL);
		lexer_eat_lines(lx, 2, NULL);
	}

	tk = lexer_emit(lx, &st, &(struct token){
	    .tk_type	= TOKEN_COMMENT,
	    .tk_flags	= c99 ? TOKEN_FLAG_COMMENT_C99 : 0,
	});

	bf = comment_trim(tk, cl->st, cl->arena.eternal_scope);
	if (bf != NULL)
		token_set_str(tk, buffer_get_ptr(bf), buffer_get_len(bf));

	/* Discard any remaining hard line(s). */
	if (block)
		lexer_eat_lines(lx, 0, NULL);

	return tk;
}

static struct token *
clang_read_cpp(struct clang *cl, struct lexer *lx)
{
	struct lexer_state cmpst, oldst, st;
	struct buffer *bf;
	struct token *tk;
	int type = TOKEN_CPP;
	int comment;
	unsigned char ch;

	oldst = st = lexer_get_state(lx);
	lexer_eat_lines_and_spaces(lx, &st);
	if (lexer_getc(lx, &ch) || ch != '#') {
		lexer_set_state(lx, &oldst);
		return NULL;
	}

	/* Space before keyword is allowed. */
	lexer_eat_spaces(lx, NULL);
	cmpst = lexer_get_state(lx);

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

	if (lexer_buffer_streq(lx, &cmpst, "ifndef"))
		type = TOKEN_CPP_IFNDEF;
	else if (lexer_buffer_streq(lx, &cmpst, "if"))
		type = TOKEN_CPP_IF;
	else if (lexer_buffer_streq(lx, &cmpst, "else") ||
	    lexer_buffer_streq(lx, &cmpst, "elif"))
		type = TOKEN_CPP_ELSE;
	else if (lexer_buffer_streq(lx, &cmpst, "endif"))
		type = TOKEN_CPP_ENDIF;
	else if (lexer_buffer_streq(lx, &cmpst, "define"))
		type = TOKEN_CPP_DEFINE;
	else if (lexer_buffer_streq(lx, &cmpst, "include"))
		type = TOKEN_CPP_INCLUDE;

	/*
	 * As cpp tokens are emitted as is, honor up to 2 hard line(s).
	 * Additional ones are excessive and will be discarded.
	 */
	lexer_eat_lines(lx, 2, NULL);

	tk = lexer_emit(lx, &st, &(struct token){
	    .tk_type	= type,
	    .tk_flags	= TOKEN_FLAG_CPP,
	});

	bf = cpp_align(tk, cl->st, cl->arena.eternal_scope, cl->arena.scratch,
	    cl->op);
	if (bf != NULL)
		token_set_str(tk, buffer_get_ptr(bf), buffer_get_len(bf));

	/* Discard any remaining hard line(s). */
	lexer_eat_lines(lx, 0, NULL);

	return tk;
}

static struct token *
clang_keyword(struct lexer *lx)
{
	struct lexer_state st;
	struct token *pv = NULL;
	struct token *tk = NULL;
	unsigned char ch;

	lexer_eat_lines_and_spaces(lx, NULL);
	st = lexer_get_state(lx);
	if (lexer_getc(lx, &ch))
		return NULL;

	for (;;) {
		struct token *tmp;

		tmp = clang_find_keyword(lx, &st);
		if (tmp == NULL) {
			lexer_ungetc(lx);
			tk = pv;
			break;
		}
		if ((tmp->tk_flags & TOKEN_FLAG_AMBIGUOUS) == 0) {
			tk = tmp;
			break;
		}

		if (tmp->tk_type == TOKEN_PERIOD) {
			struct token *ellipsis;
			unsigned char peek;

			/* Detect fractional only float literals. */
			if (lexer_getc(lx, &peek) || isdigit(peek))
				break;
			lexer_ungetc(lx);

			/* Hack to detect ellipses since ".." is not valid. */
			ellipsis = clang_ellipsis(lx, &st);
			if (ellipsis != NULL) {
				tk = ellipsis;
				break;
			}
		}

		pv = tmp;
		if (lexer_getc(lx, &ch)) {
			tk = tmp;
			break;
		}
	}
	if (tk == NULL) {
		lexer_set_state(lx, &st);
		return NULL;
	}
	return lexer_emit(lx, &st, tk);
}

static void
clang_insert_keyword(const struct token *tk)
{
	struct token *dst;
	unsigned char slot;

	slot = (unsigned char)tk->tk_str[0];
	if (table_tokens[slot] == NULL) {
		if (VECTOR_INIT(table_tokens[slot]))
			err(1, NULL);
	}
	dst = VECTOR_ALLOC(table_tokens[slot]);
	if (dst == NULL)
		err(1, NULL);
	*dst = *tk;
}

static struct token *
clang_find_keyword(const struct lexer *lx, const struct lexer_state *st)
{
	const char *key;
	size_t len;

	key = lexer_buffer_slice(lx, st, &len);
	if (key == NULL)
		return NULL;
	return clang_find_keyword1(key, len);
}

static struct token *
clang_find_keyword1(const char *key, size_t len)
{
	unsigned char slot;
	size_t i;

	slot = (unsigned char)key[0];
	if (table_tokens[slot] == NULL)
		return NULL;
	for (i = 0; i < VECTOR_LENGTH(table_tokens[slot]); i++) {
		struct token *tk = &table_tokens[slot][i];

		if (token_rawcmp(tk, key, len) == 0)
			return tk;
	}
	return NULL;
}

/*
 * Recognize aliased token which is a keyword preceded or succeeded with
 * underscores.
 */
static struct token *
clang_find_alias(struct lexer *lx, const struct lexer_state *st)
{
	const struct {
		struct {
			const char	*str;
			size_t		 len;
		} alias, keyword;
	} aliases[] = {
#define ALIAS(a, k)	{ { a, sizeof(a) - 1, }, { k, sizeof(k) - 1 } }
		ALIAS("asm",		"asm"),
		ALIAS("attribute",	"__attribute__"),
		ALIAS("inline",		"inline"),
		ALIAS("restrict",	"restrict"),
		ALIAS("volatile",	"volatile"),
#undef ALIAS
	};
	const size_t naliases = sizeof(aliases) / sizeof(aliases[0]);
	size_t i, len;
	struct token *kw;
	const char *str;
	int nunderscores = 0;

	str = lexer_buffer_slice(lx, st, &len);
	if (str == NULL)
		return NULL;
	for (; len > 0 && str[0] == '_'; len--, str++)
		nunderscores++;
	for (; len > 0 && str[len - 1] == '_'; len--)
		nunderscores++;
	if (nunderscores == 0)
		return NULL;
	for (i = 0; i < naliases; i++) {
		if (aliases[i].alias.len == len &&
		    strncmp(aliases[i].alias.str, str, len) == 0)
			break;
	}
	if (i == naliases)
		return NULL;

	kw = clang_find_keyword1(aliases[i].keyword.str,
	    aliases[i].keyword.len);
	return lexer_emit(lx, st, &(struct token){
	    .tk_type	= kw->tk_type,
	    .tk_flags	= kw->tk_flags,
	});
}

static struct token *
clang_ellipsis(struct lexer *lx, const struct lexer_state *st)
{
	struct lexer_state oldst;
	unsigned char ch;
	int i;

	oldst = lexer_get_state(lx);

	for (i = 0; i < 2; i++) {
		if (lexer_eof(lx) || lexer_getc(lx, &ch) || ch != '.') {
			lexer_set_state(lx, &oldst);
			return NULL;
		}
	}
	return clang_find_keyword(lx, st);
}

static void
token_branch_link(struct token *src, struct token *dst)
{
	src->tk_branch.br_nx = dst;
	dst->tk_branch.br_pv = src;
}

static void
token_branch_revert(struct token *tk)
{
	struct token *parent = tk->tk_branch.br_parent;

	memset(&tk->tk_branch, 0, sizeof(tk->tk_branch));
	tk->tk_type = TOKEN_CPP;
	if (parent != NULL) {
		token_branch_parent_update_flags(parent);
		token_rele(parent);
	}
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
isnum(unsigned char ch)
{
	ch = isupper(ch) ? (unsigned char)tolower(ch) : ch;
	return isdigit(ch) || isxdigit(ch) || ch == 'l' || ch == 'x' ||
	    ch == 'u' || ch == '.';
}
