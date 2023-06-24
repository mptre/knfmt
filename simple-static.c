#include "simple-static.h"

#include "config.h"

#include "lexer.h"
#include "token.h"

static struct token	*find_static(struct token *, struct token *);

struct token *
simple_static(struct lexer *lx, struct token *end)
{
	struct token *beg, *tk;

	if (!lexer_peek(lx, &beg))
		return end;
	tk = find_static(beg, end);
	if (tk == NULL || !token_is_moveable(tk))
		return end;

	if (tk == end)
		end = token_prev(end);
	lexer_move_before(lx, beg, tk);
	return end;
}

static struct token *
find_static(struct token *beg, struct token *end)
{
	struct token *tk = beg;

	for (;;) {
		if (tk->tk_type == TOKEN_STATIC) {
			if (tk == beg)
				return NULL;
			return tk;
		}
		if (tk == end)
			break;
		tk = token_next(tk);
	}
	return NULL;
}
