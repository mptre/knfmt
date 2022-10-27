#include "parser-type.h"

#include <string.h>

#include "cdefs.h"
#include "lexer.h"
#include "token.h"

static int	parser_type_peek_func_ptr(struct lexer *, struct token **);
static int	parser_type_peek_ptr(struct lexer *, struct token **);
static int	parser_type_peek_ident(struct lexer *);
static int	parser_type_peek_cpp(struct lexer *);

int
parser_type_peek(struct lexer *lx, struct token **tk, unsigned int flags)
{
	struct lexer_state s;
	struct token *beg, *t;
	int peek = 0;
	int nkeywords = 0;
	int ntokens = 0;
	int unknown = 0;

	if (!lexer_peek(lx, &beg))
		return 0;

	lexer_peek_enter(lx, &s);
	for (;;) {
		if (lexer_peek_if(lx, LEXER_EOF, NULL))
			break;

		if (lexer_if_flags(lx,
		    TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE, &t)) {
			nkeywords++;
			peek = 1;
		} else if (lexer_if_flags(lx, TOKEN_FLAG_TYPE, &t)) {
			if (t->tk_type == TOKEN_ENUM ||
			    t->tk_type == TOKEN_STRUCT ||
			    t->tk_type == TOKEN_UNION)
				lexer_if(lx, TOKEN_IDENT, &t);
			/* Recognize constructs like `struct s[]'. */
			(void)lexer_if_pair(lx, TOKEN_LSQUARE, TOKEN_RSQUARE,
			    &t);
			peek = 1;
		} else if (lexer_if(lx, TOKEN_STAR, &t)) {
			/*
			 * A pointer is expected to only be followed by another
			 * pointer or a known type. Otherwise, the following
			 * identifier cannot be part of the type.
			 */
			if (lexer_peek_if(lx, TOKEN_IDENT, NULL))
				break;
			/* A type cannot start with a pointer. */
			if (ntokens == 0)
				break;
			peek = 1;
		} else if (parser_type_peek_cpp(lx)) {
			lexer_if(lx, TOKEN_IDENT, NULL);
			lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &t);
		} else if (lexer_peek_if(lx, TOKEN_IDENT, NULL)) {
			struct lexer_state ss;
			int ident;

			/*
			 * Recognize function arguments consisting of a single
			 * type and no variable name.
			 */
			ident = 0;
			lexer_peek_enter(lx, &ss);
			if ((flags & (PARSER_TYPE_CAST | PARSER_TYPE_ARG)) &&
			    ntokens == 0 && lexer_if(lx, TOKEN_IDENT, NULL) &&
			    (lexer_if(lx, TOKEN_RPAREN, NULL) ||
			     lexer_if(lx, TOKEN_COMMA, NULL)))
				ident = 1;
			lexer_peek_leave(lx, &ss);
			if (ident) {
				if (lexer_pop(lx, &t))
					peek = 1;
				break;
			}

			/* Ensure this is not the identifier after the type. */
			if ((flags & PARSER_TYPE_CAST) == 0 &&
			    parser_type_peek_ident(lx))
				break;

			/* Identifier is part of the type, consume it. */
			lexer_if(lx, TOKEN_IDENT, &t);
		} else if (ntokens > 0 && parser_type_peek_func_ptr(lx, &t)) {
			struct token *align;

			/*
			 * Instruct parser_exec_type() where to perform ruler
			 * alignment.
			 */
			if (lexer_back(lx, &align))
				t->tk_token = align;
			peek = 1;
			break;
		} else if (ntokens > 0 && parser_type_peek_ptr(lx, &t)) {
			peek = 1;
			break;
		} else {
			unknown = 1;
			break;
		}

		ntokens++;
	}
	lexer_peek_leave(lx, &s);

	if (ntokens > 0 && ntokens == nkeywords &&
	    (flags & PARSER_TYPE_ARG) == 0) {
		/* Only qualifier or storage token(s) cannot denote a type. */
		peek = 0;
	} else if (!peek && !unknown && ntokens > 0) {
		/*
		 * Nothing was found. However this is a sequence of identifiers
		 * (i.e. unknown types) therefore treat it as a type.
		 */
		peek = 1;
	}

	if (peek && tk != NULL)
		*tk = t;
	return peek;
}

int
parser_type_exec(struct lexer *lx, struct token **tk, unsigned int flags)
{
	struct token *t;

	if (!parser_type_peek(lx, &t, flags))
		return 0;
	lexer_seek(lx, t);
	if (tk != NULL)
		*tk = t;
	return 1;
}

static int
parser_type_peek_func_ptr(struct lexer *lx, struct token **tk)
{
	struct lexer_state s;
	struct token *lparen, *rparen;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_LPAREN, NULL) &&
	    lexer_if(lx, TOKEN_STAR, NULL)) {
		struct token *ident = NULL;

		while (lexer_if(lx, TOKEN_STAR, NULL))
			continue;

		lexer_if_flags(lx, TOKEN_FLAG_QUALIFIER, NULL);
		lexer_if(lx, TOKEN_IDENT, &ident);
		if (lexer_if(lx, TOKEN_LSQUARE, NULL)) {
			lexer_if(lx, TOKEN_LITERAL, NULL);
			lexer_if(lx, TOKEN_RSQUARE, NULL);
		}
		if (lexer_if(lx, TOKEN_RPAREN, &rparen)) {
			if (lexer_peek_if(lx, TOKEN_LPAREN, &lparen) &&
			    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, tk)) {
				/*
				 * Annotate the left parenthesis, used by
				 * parser_exec_type().
				 */
				lparen->tk_flags |= TOKEN_FLAG_TYPE_ARGS;

				peek = 1;
			} else if (ident == NULL &&
			    (lexer_if(lx, TOKEN_RPAREN, NULL) ||
			     lexer_if(lx, LEXER_EOF, NULL))) {
				/*
				 * Function pointer lacking arguments wrapped in
				 * parenthesis. Be careful here in order to not
				 * confuse a function call.
				 */
				peek = 1;
				*tk = rparen;
			}
		}
	}
	lexer_peek_leave(lx, &s);

	return peek;
}

static int
parser_type_peek_ptr(struct lexer *lx, struct token **UNUSED(tk))
{
	struct lexer_state s;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_LPAREN, NULL) &&
	    lexer_if(lx, TOKEN_STAR, NULL) &&
	    lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if(lx, TOKEN_RPAREN, NULL) &&
	    lexer_if(lx, TOKEN_LSQUARE, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);
	return peek;
}

static int
parser_type_peek_ident(struct lexer *lx)
{
	struct lexer_state s;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, NULL) &&
	    (lexer_if_flags(lx, TOKEN_FLAG_ASSIGN, NULL) ||
	     lexer_if(lx, TOKEN_LSQUARE, NULL) ||
	     (lexer_if(lx, TOKEN_LPAREN, NULL) &&
	      !lexer_peek_if(lx, TOKEN_STAR, NULL)) ||
	     lexer_if(lx, TOKEN_RPAREN, NULL) ||
	     lexer_if(lx, TOKEN_SEMI, NULL) ||
	     lexer_if(lx, TOKEN_COMMA, NULL) ||
	     lexer_if(lx, TOKEN_COLON, NULL) ||
	     lexer_if(lx, TOKEN_ATTRIBUTE, NULL)))
		peek = 1;
	lexer_peek_leave(lx, &s);

	return peek;
}

/*
 * Detect usage of types hidden behind cpp such as STACK_OF(X509).
 */
static int
parser_type_peek_cpp(struct lexer *lx)
{
	struct lexer_state s;
	struct token *ident;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if(lx, TOKEN_LPAREN, NULL) &&
	    lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if(lx, TOKEN_RPAREN, NULL)) {
		struct token *nx;

		if (lexer_if(lx, TOKEN_IDENT, &nx)) {
			size_t len = nx->tk_len;

			/* Ugly, do not confuse cppx. */
			if (!lexer_peek_if(lx, TOKEN_LPAREN, NULL) &&
			    (ident->tk_len != len ||
			     strncmp(ident->tk_str, nx->tk_str, len) != 0))
				peek = 1;
		} else if (lexer_if(lx, TOKEN_STAR, NULL)) {
			peek = 1;
		}
	}
	lexer_peek_leave(lx, &s);
	return peek;
}
