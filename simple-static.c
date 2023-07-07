#include "simple-static.h"

#include "config.h"

#include "lexer.h"
#include "token.h"

struct token *
simple_static(struct lexer *lx, struct token *beg, struct token *end,
    struct token *tk)
{
	if (!token_is_moveable(tk))
		return end;
	if (tk == end)
		end = token_prev(end);
	lexer_move_before(lx, beg, tk);
	return end;
}
