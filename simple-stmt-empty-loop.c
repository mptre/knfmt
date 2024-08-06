#include "simple-stmt-empty-loop.h"

#include "config.h"

#include "clang.h"
#include "lexer.h"
#include "token.h"

static int
sense_empty_loop_braces(const struct token *lbrace, const struct token *rbrace)
{
	const struct token *nx;

	if (!token_is_moveable(lbrace) || !token_is_moveable(rbrace))
		return 0;

	nx = token_next(lbrace);
	if (nx == rbrace)
		return 1;
	if (nx->tk_type == TOKEN_SEMI && token_is_moveable(nx) &&
	    token_next(nx) == rbrace)
		return 1;
	return 0;
}

static struct token *
insert_continue(struct lexer *lx, struct token *after)
{
	return lexer_insert_after(lx, after,
	    clang_keyword_token(TOKEN_CONTINUE));
}

static struct token *
insert_semi(struct lexer *lx, struct token *after)
{
	return lexer_insert_after(lx, after, clang_keyword_token(TOKEN_SEMI));
}

void
simple_stmt_empty_loop_braces(struct lexer *lx)
{
	struct token *after, *lbrace, *nx, *rbrace;

	if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &lbrace,
	    &rbrace))
		return;
	if (!sense_empty_loop_braces(lbrace, rbrace))
		return;

	after = insert_continue(lx, lbrace);
	nx = token_next(after);
	if (nx->tk_type != TOKEN_SEMI)
		insert_semi(lx, after);
}

void
simple_stmt_empty_loop_no_braces(struct lexer *lx)
{
	struct token *after;

	if (!lexer_peek_if(lx, TOKEN_SEMI, NULL) ||
	    !lexer_back(lx, &after))
		return;

	insert_continue(lx, after);
}
