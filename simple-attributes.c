#include "simple-attributes.h"

#include "config.h"

#include <string.h>

#include "libks/arena.h"

#include "lexer.h"
#include "token.h"

static const char *
remove_underscores(const char *str, size_t len, struct arena_scope *s)
{
	return arena_strndup(s, &str[2], len - 4);
}

static int
has_underscores(const struct token *tk)
{
	const char *str = tk->tk_str;
	size_t len = tk->tk_len;

	return len > 2 &&
	    strncmp(str, "__", 2) == 0 &&
	    strncmp(&str[len - 2], "__", 2) == 0;
}

void
simple_attributes(struct lexer *lx, struct arena_scope *s)
{
	const char *sanitized_ident;
	struct token *ident;

	if (!lexer_peek_if(lx, TOKEN_IDENT, &ident) || !has_underscores(ident))
		return;

	sanitized_ident = remove_underscores(ident->tk_str, ident->tk_len, s);
	token_set_str(ident, sanitized_ident, strlen(sanitized_ident));
}
