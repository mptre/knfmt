#include "simple-static.h"

#include "config.h"

#include "lexer.h"
#include "token.h"

static struct token *
find_static_token(struct token *beg, struct token *end)
{
	struct token *tk = beg;

	for (;;) {
		if (tk->tk_type == TOKEN_STATIC)
			return tk;
		if (tk == end)
			break;
		tk = token_next(tk);
	}
	return NULL;
}

struct token *
simple_static(struct lexer *lx, struct token *beg, struct token *end)
{
	struct token *tk;

	tk = find_static_token(beg, end);
	if (tk == NULL || tk == beg || !token_is_moveable(tk))
		return end;
	if (tk == end)
		end = token_prev(end);
	lexer_move_before(lx, beg, tk);
	return end;
}
