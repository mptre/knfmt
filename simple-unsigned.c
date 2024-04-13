#include "simple-unsigned.h"

#include "config.h"

#include "clang.h"
#include "lexer.h"
#include "token.h"

static int
is_unsigned(const struct token *beg, const struct token *end)
{
	return beg == end && beg->tk_type == TOKEN_UNSIGNED &&
	    token_is_moveable(beg);
}

struct token *
simple_unsigned(struct lexer *lx, struct token *beg, struct token *end)
{
	if (!is_unsigned(beg, end))
		return end;

	return lexer_insert_after(lx, end, clang_keyword_token(TOKEN_INT));
}
