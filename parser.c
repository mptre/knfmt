#include "parser.h"

#include "config.h"

#include "libks/arena-buffer.h"
#include "libks/arena.h"
#include "libks/compiler.h"

#include "doc.h"
#include "lexer.h"
#include "options.h"
#include "parser-decl.h"
#include "parser-extern.h"
#include "parser-func.h"
#include "parser-priv.h"
#include "parser-stmt-asm.h"
#include "token.h"

static int
parser_get_error(const struct parser *pr)
{
	return pr->pr_error || lexer_get_error(pr->pr_lx);
}

struct parser *
parser_alloc(struct lexer *lx, const struct style *st, struct simple *si,
    struct arena_scope *eternal_scope, struct arena *scratch,
    struct arena *doc, const struct options *op)
{
	struct parser *pr;

	pr = arena_calloc(eternal_scope, 1, sizeof(*pr));
	pr->pr_st = st;
	pr->pr_si = si;
	pr->pr_op = op;
	pr->pr_lx = lx;
	pr->pr_bf = arena_buffer_alloc(eternal_scope, 1 << 10);
	pr->pr_arena.scratch = scratch;
	pr->pr_arena.doc = doc;

	return pr;
}

int
parser_exec(struct parser *pr, const struct diffchunk *diff_chunks,
    struct buffer *bf)
{
	struct doc *dc;
	struct lexer *lx = pr->pr_lx;
	unsigned int doc_flags = 0;
	int error = 0;

	arena_scope(pr->pr_arena.doc, doc_scope);
	parser_arena_scope(&pr->pr_arena.doc_scope, &doc_scope);

	dc = doc_root(&doc_scope);

	for (;;) {
		struct doc *concat;
		struct token *tk;

		concat = doc_alloc(DOC_CONCAT, dc);

		/* Always emit EOF token as it could have dangling tokens. */
		if (lexer_if(lx, LEXER_EOF, &tk)) {
			struct doc *eof;

			eof = doc_alloc(DOC_CONCAT,
			    doc_alloc(DOC_GROUP, concat));
			doc_token(tk, eof);
			error = 0;
			break;
		}

		error = parser_exec1(pr, concat);
		if (error & GOOD) {
			lexer_stamp(lx);
		} else if (error & BRCH) {
			if (!lexer_branch(lx))
				break;
			parser_reset(pr);
		} else if (error & (FAIL | NONE)) {
			int r;

			r = lexer_recover(lx);
			if (r == 0)
				break;
			while (r-- > 0)
				doc_remove_tail(dc);
			parser_reset(pr);
		}
	}
	if (error) {
		parser_fail(pr);
		doc_free(dc);
		return 1;
	}

	if (pr->pr_op->diffparse)
		doc_flags |= DOC_EXEC_DIFF;
	else
		doc_flags |= DOC_EXEC_TRIM;
	if (trace(pr->pr_op, 'd'))
		doc_flags |= DOC_EXEC_TRACE;
	doc_exec(&(struct doc_exec_arg){
	    .dc			= dc,
	    .lx			= pr->pr_op->diffparse ? pr->pr_lx : NULL,
	    .scratch		= pr->pr_arena.scratch,
	    .diff_chunks	= pr->pr_op->diffparse ? diff_chunks : NULL,
	    .bf			= bf,
	    .st			= pr->pr_st,
	    .op			= pr->pr_op,
	    .flags		= doc_flags,
	});

	doc_free(dc);
	return 0;
}

int
parser_exec1(struct parser *pr, struct doc *dc)
{
	int error;

	error = parser_decl(pr, dc,
	    PARSER_DECL_BREAK | PARSER_DECL_LINE |
	    PARSER_DECL_ROOT | PARSER_DECL_TRIM_SEMI |
	    PARSER_DECL_SIMPLE_FORWARD);
	if (error & NONE)
		error = parser_func_impl(pr, dc);
	if (error & NONE)
		error = parser_extern(pr, dc);
	if (error & NONE)
		error = parser_asm(pr, dc);
	return error;
}

int
parser_fail0(struct parser *pr, const char *fun, int lno)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk = NULL;

	if (parser_get_error(pr))
		goto out;
	pr->pr_error = 1;

	(void)lexer_back(lx, &tk);
	lexer_error(pr->pr_lx, tk, fun, lno,
	    "error at %s", lexer_serialize(lx, tk));

out:
	if (lexer_is_branch(pr->pr_lx))
		return BRCH;
	return FAIL;
}

/*
 * Remove any preceding hard line(s) from the given token, unless the token has
 * prefixes intended to be emitted.
 */
void
parser_token_trim_before(const struct parser *UNUSED(pr), struct token *tk)
{
	struct token *pv;

	if (!token_is_moveable(tk))
		return;
	pv = token_prev(tk);
	if (pv != NULL)
		token_trim(pv);
}

/*
 * Remove any subsequent hard line(s) from the given token.
 */
void
parser_token_trim_after(const struct parser *UNUSED(pr), struct token *tk)
{
	token_trim(tk);
}

/*
 * Returns the width of the given document.
 */
unsigned int
parser_width(struct parser *pr, const struct doc *dc)
{
	return doc_width(&(struct doc_exec_arg){
	    .dc		= dc,
	    .scratch	= pr->pr_arena.scratch,
	    .bf		= pr->pr_bf,
	    .st		= pr->pr_st,
	    .op		= pr->pr_op,
	});
}

int
parser_good(const struct parser *pr)
{
	if (lexer_is_branch(pr->pr_lx))
		return BRCH;
	return parser_get_error(pr) ? FAIL : GOOD;
}

int
parser_none(const struct parser *pr)
{
	if (lexer_is_branch(pr->pr_lx))
		return BRCH;
	return parser_get_error(pr) ? FAIL : NONE;
}

void
parser_reset(struct parser *pr)
{
	lexer_error_reset(pr->pr_lx);
	pr->pr_error = 0;
}

void
parser_arena_scope_enter(struct parser_doc_scope_cookie *cookie,
    struct arena_scope **old_scope, struct arena_scope *new_scope)
{
	cookie->restore_scope = old_scope;
	cookie->old_scope = *old_scope;
	*old_scope = new_scope;
}

void
parser_arena_scope_leave(struct parser_doc_scope_cookie *cookie)
{
	*cookie->restore_scope = cookie->old_scope;
}
