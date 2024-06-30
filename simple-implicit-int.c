#include "simple-implicit-int.h"

#include "config.h"

#include "clang.h"
#include "lexer.h"
#include "token.h"

static int
is_implicit_int(const struct token *beg, const struct token *end,
    int token_type)
{
	return beg == end && beg->tk_type == token_type &&
	    token_is_moveable(beg);
}

struct token *
simple_implicit_int(struct lexer *lx, struct token *beg, struct token *end)
{
	if (!is_implicit_int(beg, end, TOKEN_UNSIGNED) &&
	    !is_implicit_int(beg, end, TOKEN_SIGNED))
		return end;

	return lexer_insert_after(lx, end, clang_keyword_token(TOKEN_INT));
}
