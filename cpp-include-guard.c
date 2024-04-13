#include "cpp-include-guard.h"

#include "config.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "libks/arena-buffer.h"
#include "libks/arena.h"
#include "libks/buffer.h"

#include "clang.h"
#include "lexer.h"
#include "style.h"
#include "token.h"
#include "util.h"

struct include_guard_context {
	struct {
		struct token	*parent;
		struct token	*tk;
	} ifndef, define, endif;
};

static int
is_header(const char *path)
{
	const char needle[] = ".h";
	const char *dot;

	dot = strrchr(path, '.');
	if (dot == NULL)
		return 0;
	return dot[sizeof(needle) - 1] == '\0' && strcmp(dot, needle) == 0;
}

static int
is_guard_define(const struct token *tk)
{
	const char *str = tk->tk_str;
	size_t len = tk->tk_len;
	int nwords = 0;
	int threshold = 2;

	while (len > 0) {
		int word = 0;

		for (; !isspace((unsigned char)str[0]) && len > 0; str++, len--)
			word = 1;
		nwords += word;
		if (nwords > threshold)
			break;

		for (; isspace((unsigned char)str[0]) && len > 0; str++, len--)
			continue;
	}

	return nwords == threshold;
}

static const char *
path_to_guard(const char *path, unsigned int ncomponents, struct arena_scope *s)
{
	struct buffer *bf;
	const char *sliced_path;
	char *resolved_path;

	resolved_path = arena_malloc(s, PATH_MAX);
	if (realpath(path, resolved_path) == NULL)
		return NULL;
	sliced_path = path_slice(resolved_path, ncomponents);

	bf = arena_buffer_alloc(s, PATH_MAX);
	for (; sliced_path[0] != '\0'; sliced_path++) {
		char c = sliced_path[0];

		if (c == '.' || c == '/' || c == '-')
			buffer_putc(bf, '_');
		else
			buffer_putc(bf, toupper(c));
	}
	return buffer_str(bf);
}

static int
sense_include_guards(struct lexer *lx, const char *cpp_ifndef,
    const char *cpp_define, const char *cpp_endif,
    struct include_guard_context *c)
{
	struct token *define, *endif, *eof, *first, *ifndef;

	if (!lexer_peek(lx, &first) || !lexer_peek_last(lx, &eof))
		return -1;

	c->ifndef.parent = first;
	c->define.parent = first;
	c->endif.parent = eof;

	ifndef = token_list_find(&first->tk_prefixes, TOKEN_CPP_IFNDEF, 0);
	if (ifndef == NULL)
		return 0;

	define = token_list_find(&first->tk_prefixes, TOKEN_CPP_DEFINE, 0);
	if (define == NULL ||
	    token_next(ifndef) != define ||
	    !is_guard_define(define))
		return 0;

	endif = clang_token_branch_next(ifndef);
	if (endif->tk_type != TOKEN_CPP_ENDIF ||
	    clang_token_branch_parent(endif) != eof)
		return 0;

	c->ifndef.tk = ifndef;
	c->define.tk = define;
	c->endif.tk = endif;

	return token_rawcmp(ifndef, cpp_ifndef, strlen(cpp_ifndef)) == 0 &&
	    token_rawcmp(define, cpp_define, strlen(cpp_define)) == 0 &&
	    token_rawcmp(endif, cpp_endif, strlen(cpp_endif)) == 0;
}

static struct token *
emit_cpp(struct lexer *lx, int token_type, const char *str)
{
	struct lexer_state ls;

	/* Note, line and column will be off. */
	ls = lexer_get_state(lx);
	return lexer_emit(lx, &ls, &(struct token){
	    .tk_type		= token_type,
	    .tk_flags		= TOKEN_FLAG_CPP,
	    .tk_str		= str,
	    .tk_len		= strlen(str),
	});
}

static struct token *
emit_line(struct lexer *lx)
{
	struct lexer_state ls;

	/* Note, line and column will be off. */
	ls = lexer_get_state(lx);
	return lexer_emit(lx, &ls, &(struct token){
	    .tk_type = TOKEN_SPACE,
	    .tk_str = "\n",
	    .tk_len = 1,
	});
}

static struct token *
emit_ifndef(struct lexer *lx, struct token *tk, const char *cpp)
{
	struct token *comment = NULL;
	struct token *ifndef, *prefix;

	ifndef = emit_cpp(lx, TOKEN_CPP, cpp);

	/* Allow one or many comments before the include guard. */
	for (prefix = token_list_first(&tk->tk_prefixes); prefix != NULL;
	    prefix = token_next(prefix)) {
		if (prefix->tk_type == TOKEN_COMMENT)
			comment = prefix;
		else
			break;
	}

	if (comment != NULL) {
		if (token_has_verbatim_line(comment, 2)) {
			token_list_append_after(&tk->tk_prefixes, comment,
			    ifndef);
		} else {
			struct token *line;

			line = emit_line(lx);
			token_list_append_after(&tk->tk_prefixes, comment,
			    line);
			token_list_append_after(&tk->tk_prefixes, line, ifndef);
		}
	} else {
		token_list_prepend(&tk->tk_prefixes, ifndef);
	}

	return ifndef;
}

static void
ensure_line(struct lexer *lx, struct token *eof)
{
	if (token_has_prefixes(eof)) {
		struct token *last;

		last = token_list_last(&eof->tk_prefixes);
		if (last != NULL && !token_has_verbatim_line(last, 2))
			token_list_append(&eof->tk_prefixes, emit_line(lx));
	} else {
		struct token *pv;

		pv = token_prev(eof);
		if (pv != NULL)
			token_trim(pv);
		token_list_append(&eof->tk_prefixes, emit_line(lx));
	}
}

void
cpp_include_guard(const struct style *st, struct lexer *lx,
    struct arena_scope *eternal_scope, struct arena *scratch)
{
	struct include_guard_context c = {0};
	struct token *define, *endif, *ifndef;
	const char *cpp_define, *cpp_endif, *cpp_ifndef, *guard, *path;

	path = lexer_get_path(lx);
	if (style(st, IncludeGuards) == 0 || !is_header(path))
		return;

	arena_scope(scratch, s);

	guard = path_to_guard(path, style(st, IncludeGuards), &s);
	if (guard == NULL)
		return;
	cpp_ifndef = arena_sprintf(eternal_scope, "#ifndef %s\n", guard);
	cpp_define = arena_sprintf(eternal_scope, "#define %s\n\n", guard);
	cpp_endif = arena_sprintf(eternal_scope, "#endif /* !%s */\n", guard);

	if (sense_include_guards(lx, cpp_ifndef, cpp_define, cpp_endif, &c))
		return;

	/*
	 * Intentionally not creating a cpp branch as recovering from it won't
	 * make a difference.
	 */
	if (c.ifndef.tk != NULL) {
		clang_token_branch_unlink(c.ifndef.tk);
		token_list_remove(&c.ifndef.parent->tk_prefixes, c.ifndef.tk);
	}
	ifndef = emit_ifndef(lx, c.ifndef.parent, cpp_ifndef);

	if (c.define.tk != NULL)
		token_list_remove(&c.define.parent->tk_prefixes, c.define.tk);
	define = emit_cpp(lx, TOKEN_CPP_DEFINE, cpp_define);
	token_list_append_after(&c.define.parent->tk_prefixes, ifndef,
	    define);

	ensure_line(lx, c.endif.parent);

	if (c.endif.tk != NULL) {
		clang_token_branch_unlink(c.endif.tk);
		token_list_remove(&c.endif.parent->tk_prefixes, c.endif.tk);
	}
	endif = emit_cpp(lx, TOKEN_CPP, cpp_endif);
	token_list_append(&c.endif.parent->tk_prefixes, endif);
}
