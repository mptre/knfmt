/* Ensure storage keywords such as extern and static comes first as part of a
 * type declaration. */

#include "simple-storage.h"

#include "config.h"

#include "lexer.h"
#include "token.h"

static struct token *
find_token(int token_type, struct token *beg, struct token *end)
{
	struct token *tk = beg;

	for (;;) {
		if (tk->tk_type == token_type)
			return tk;
		if (tk == end)
			break;
		tk = token_next(tk);
	}
	return NULL;
}

static int
move_storage_token(struct lexer *lx, int token_type, struct token *beg, struct token *end,
    struct token **out)
{
	struct token *tk = find_token(token_type, beg, end);
	if (tk == NULL || tk == beg || !token_is_moveable(tk))
		return 0;

	*out = tk == end ? token_prev(end) : end;
	lexer_move_before(lx, beg, tk);
	return 1;
}

struct token *
simple_storage(struct lexer *lx, struct token *beg, struct token *end)
{
	struct token *out;
	if (move_storage_token(lx, TOKEN_STATIC, beg, end, &out))
		end = out;
	else if (move_storage_token(lx, TOKEN_EXTERN, beg, end, &out))
		end = out;
	return end;
}
