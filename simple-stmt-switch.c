#include "simple-stmt-switch.h"

#include "config.h"

#include "clang.h"
#include "lexer.h"
#include "token.h"

static void
token_move_to_next_line(struct token *tk)
{
	tk->tk_lno++;
}

void
simple_stmt_switch(struct lexer *lx, struct token *tkcase)
{
	struct lexer_state ls;
	struct token *colon, *tkbreak;
	int peek = 0;

	if (tkcase->tk_type != TOKEN_DEFAULT)
		return;

	lexer_peek_enter(lx, &ls);
	lexer_seek(lx, tkcase);
	peek = lexer_if(lx, TOKEN_DEFAULT, NULL) &&
	    lexer_if(lx, TOKEN_COLON, &colon) &&
	    lexer_if(lx, TOKEN_SEMI, NULL);
	lexer_peek_leave(lx, &ls);
	if (!peek)
		return;

	tkbreak = lexer_insert_after(lx, colon,
	    clang_keyword_token(TOKEN_BREAK));
	/* Ensure the break statement ends up on a new line. */
	token_move_to_next_line(tkbreak);
}
