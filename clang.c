#include "clang.h"

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libks/buffer.h"
#include "libks/vector.h"

#include "alloc.h"
#include "comment.h"
#include "cpp-align.h"
#include "cpp-include.h"
#include "lexer.h"
#include "options.h"
#include "token.h"
#include "util.h"

#ifdef HAVE_QUEUE
#  include <sys/queue.h>
#else
#  include "compat-queue.h"
#endif

#define clang_trace(cl, fmt, ...) do {					\
	if (trace((cl)->op, 'c'))					\
		tracef('C', __func__, (fmt), __VA_ARGS__);		\
} while (0)

struct clang {
	const struct style	*st;
	const struct options	*op;
	struct cpp_include	*ci;
	VECTOR(struct token *)	 branches;
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

static struct token	*clang_read_prefix(struct clang *, struct lexer *,
    struct token_list *);
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

static void	token_branch_link(struct token *, struct token *);
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
clang_alloc(const struct style *st, struct simple *si, struct arena *scratch,
    const struct options *op)
{
	struct clang *cl;

	cl = ecalloc(1, sizeof(*cl));
	cl->st = st;
	cl->op = op;
	cl->ci = cpp_include_alloc(st, si, scratch, op);
	if (VECTOR_INIT(cl->branches))
		err(1, NULL);
	return cl;
}

void
clang_free(struct clang *cl)
{
	if (cl == NULL)
		return;

	cpp_include_free(cl->ci);
	VECTOR_FREE(cl->branches);
	free(cl);
}

struct token *
clang_read(struct lexer *lx, void *arg)
{
	struct clang *cl = arg;
	struct token *prefix, *t, *tk, *tmp;
	struct lexer_state st;
	struct token_list prefixes;
	int ncomments = 0;
	int nlines;
	unsigned char ch;

	TAILQ_INIT(&prefixes);

	/* Consume all comments and preprocessor directives. */
	cpp_include_enter(cl->ci, lx, &prefixes);
	for (;;) {
		prefix = clang_read_prefix(cl, lx, &prefixes);
		if (prefix == NULL)
			break;
		cpp_include_add(cl->ci, prefix);
	}
	cpp_include_leave(cl->ci);

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
		do {
			if (lexer_getc(lx, &ch))
				goto eof;
		} while (isalnum(ch) || ch == '_');
		lexer_ungetc(lx);

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
	TAILQ_CONCAT(&tk->tk_prefixes, &prefixes, tk_entry);

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
	if (tk->tk_type == LEXER_EOF)
		clang_branch_purge(cl, lx);

	return tk;
}

struct token *
clang_token_alloc(struct arena_scope *s, const struct token *def)
{
	return token_alloc(s, 0, def);
}

static void
clang_branch_enter(struct clang *cl, struct lexer *lx, struct token *cpp,
    struct token *tk)
{
	struct token **br;

	clang_trace(cl, "%s", lexer_serialize(lx, cpp));
	cpp->tk_branch.br_parent = tk;
	br = VECTOR_ALLOC(cl->branches);
	if (br == NULL)
		err(1, NULL);
	*br = cpp;
}

static void
clang_branch_link(struct clang *cl, struct lexer *lx, struct token *cpp,
    struct token *tk)
{
	struct token **last;
	struct token *br;

	/* Silently ignore broken branch. */
	last = VECTOR_LAST(cl->branches);
	if (last == NULL) {
		token_branch_unlink(cpp);
		return;
	}
	br = *last;

	/*
	 * Discard branches hanging of the same token, such branch cannot cause
	 * removal of any tokens.
	 */
	if (br->tk_branch.br_parent == tk) {
		token_branch_unlink(cpp);
		return;
	}

	clang_trace(cl, "%s -> %s",
	    lexer_serialize(lx, br), lexer_serialize(lx, cpp));

	cpp->tk_branch.br_parent = tk;
	token_branch_link(br, cpp);
	*last = cpp;
}

static void
clang_branch_leave(struct clang *cl, struct lexer *lx, struct token *cpp,
    struct token *tk)
{
	struct token **last;
	struct token *br;

	/* Silently ignore broken branch. */
	last = VECTOR_LAST(cl->branches);
	if (last == NULL) {
		token_branch_unlink(cpp);
		return;
	}
	br = *last;

	/*
	 * Discard branches hanging of the same token, such branch cannot cause
	 * removal of any tokens.
	 */
	if (br->tk_branch.br_parent == tk) {
		struct token *pv;

		clang_trace(cl, "%s -> %s, discard empty branch",
		    lexer_serialize(lx, br), lexer_serialize(lx, cpp));

		/*
		 * Prevent the previous branch from being exhausted if we're
		 * about to link it again below.
		 */
		pv = br->tk_branch.br_pv;
		if (pv != NULL)
			br->tk_branch.br_pv = NULL;
		token_branch_unlink(br);

		/*
		 * If this is an empty else branch, try to link with the
		 * previous one instead.
		 */
		br = pv;
	}

	if (br != NULL) {
		cpp->tk_branch.br_parent = tk;
		token_branch_link(br, cpp);
		clang_trace(cl, "%s -> %s",
		    lexer_serialize(lx, br),
		    lexer_serialize(lx, cpp));
	} else {
		token_branch_unlink(cpp);
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
			while (token_branch_unlink(tk) == 0)
				continue;
			tk = pv;
		} while (tk != NULL);
	}
}

static struct token *
clang_read_prefix(struct clang *cl, struct lexer *lx,
    struct token_list *prefixes)
{
	struct token *comment, *cpp;

	comment = clang_read_comment(cl, lx, 1);
	if (comment != NULL) {
		struct token *pv;

		pv = TAILQ_LAST(prefixes, token_list);
		if (pv != NULL &&
		    pv->tk_type == TOKEN_COMMENT &&
		    token_cmp(comment, pv) == 0) {
			token_prolong(pv, comment);
			return pv;
		}
		token_list_append(prefixes, comment);
		return comment;
	}

	cpp = clang_read_cpp(cl, lx);
	if (cpp != NULL) {
		token_list_append(prefixes, cpp);
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

	bf = comment_trim(tk, cl->st);
	if (bf != NULL) {
		tk->tk_flags |= TOKEN_FLAG_DIRTY;
		tk->tk_len = buffer_get_len(bf);
		tk->tk_str = buffer_release(bf);
	}
	buffer_free(bf);

	/* Discard any remaining hard line(s). */
	if (block)
		lexer_eat_lines(lx, 0, NULL);

	return tk;
}

static struct token *
clang_read_cpp(struct clang *cl, struct lexer *lx)
{
	struct lexer_state cmpst, oldst, st;
	struct token *tk;
	char *str;
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

	if (lexer_buffer_streq(lx, &cmpst, "if"))
		type = TOKEN_CPP_IF;
	else if (lexer_buffer_streq(lx, &cmpst, "else") ||
	    lexer_buffer_streq(lx, &cmpst, "elif"))
		type = TOKEN_CPP_ELSE;
	else if (lexer_buffer_streq(lx, &cmpst, "endif"))
		type = TOKEN_CPP_ENDIF;
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

	str = cpp_align(tk, cl->st, cl->op);
	if (str != NULL) {
		tk->tk_flags |= TOKEN_FLAG_DIRTY;
		tk->tk_str = str;
		tk->tk_len = strlen(str);
	}

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

		if (len == tk->tk_len && strncmp(tk->tk_str, key, len) == 0)
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
