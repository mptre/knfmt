#include "parser-type.h"

#include "doc.h"
#include "lexer.h"
#include "options.h"
#include "parser-func.h"
#include "parser-priv.h"
#include "ruler.h"
#include "style.h"
#include "token.h"

int
parser_type(struct parser *pr, struct doc *dc, const struct token *end,
    struct ruler *rl)
{
	struct parser_private *pp = parser_get_private(pr);
	struct lexer *lx = pp->lx;
	const struct token *align = NULL;
	struct token *beg;
	unsigned int nspaces = 0;

	if (!lexer_peek(lx, &beg))
		return parser_fail(pr);

	if (rl != NULL) {
		/*
		 * Find the first non pointer token starting from the end, this
		 * is where the ruler alignment must be performed.
		 */
		align = end->tk_token != NULL ? end->tk_token : end;
		for (;;) {
			if (align->tk_type != TOKEN_STAR)
				break;

			nspaces++;
			if (align == beg)
				break;
			align = token_prev(align);
			if (align == NULL)
				break;
		}

		/*
		 * No alignment wanted if the first non-pointer token is
		 * followed by a semi.
		 */
		if (align != NULL) {
			const struct token *nx;

			nx = token_next(align);
			if (nx != NULL && nx->tk_type == TOKEN_SEMI)
				align = NULL;
		}
	}

	if (pp->op->op_flags & OPTIONS_SIMPLE) {
		struct token *tk = beg;
		int ntokens = 0;

		for (;;) {
			struct token *nx;

			nx = token_next(tk);
			if (ntokens > 0 &&
			    tk->tk_type == TOKEN_STATIC &&
			    token_is_moveable(tk)) {
				token_move_prefixes(beg, tk);
				token_move_suffixes(tk, beg);
				lexer_move_before(lx, beg, tk);
			}
			if (tk == end)
				break;
			tk = nx;
			ntokens++;
		}
	}

	for (;;) {
		struct doc *concat;
		struct token *tk;
		int didalign = 0;

		if (!lexer_pop(lx, &tk))
			return parser_fail(pr);
		parser_token_trim_after(pr, tk);

		if (tk->tk_flags & TOKEN_FLAG_TYPE_ARGS) {
			struct doc *indent;
			struct token *lparen = tk;
			struct token *rparen;
			unsigned int w;

			doc_token(lparen, dc);
			if (style(pp->st, AlignAfterOpenBracket) == Align)
				w = parser_width(pr, dc);
			else
				w = style(pp->st, ContinuationIndentWidth);
			indent = doc_alloc_indent(w, dc);
			while (parser_func_arg(pr, indent, NULL, end) & GOOD)
				continue;
			if (lexer_expect(lx, TOKEN_RPAREN, &rparen))
				doc_token(rparen, dc);
			break;
		}

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_token(tk, concat);

		if (tk == align) {
			if (token_is_decl(tk, TOKEN_ENUM) ||
			    token_is_decl(tk, TOKEN_STRUCT) ||
			    token_is_decl(tk, TOKEN_UNION)) {
				doc_alloc(DOC_LINE, concat);
			} else {
				ruler_insert(rl, tk, concat, 1,
				    parser_width(pr, dc), nspaces);
			}
			didalign = 1;
		}

		if (tk == end)
			break;

		if (!didalign) {
			struct lexer_state s;
			struct token *nx;

			lexer_peek_enter(lx, &s);
			if (tk->tk_type != TOKEN_STAR &&
			    tk->tk_type != TOKEN_LPAREN &&
			    tk->tk_type != TOKEN_LSQUARE &&
			    lexer_pop(lx, &nx) &&
			    (nx->tk_type != TOKEN_LPAREN ||
			     lexer_if(lx, TOKEN_STAR, NULL)) &&
			    nx->tk_type != TOKEN_LSQUARE &&
			    nx->tk_type != TOKEN_RSQUARE &&
			    nx->tk_type != TOKEN_RPAREN &&
			    nx->tk_type != TOKEN_COMMA)
				doc_alloc(DOC_LINE, concat);
			lexer_peek_leave(lx, &s);
		}
	}

	return parser_good(pr);
}
