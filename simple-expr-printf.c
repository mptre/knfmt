#include "simple-expr-printf.h"

#include <string.h>

#include "libks/arena.h"

#include "lexer.h"
#include "token.h"

enum format_argno {
	FORMAT_ARGNO_0 = 0,
	FORMAT_ARGNO_1 = 1,
	FORMAT_ARGNO_2 = 2,
};

static int
identify_function(const struct token *tk, enum format_argno *out)
{
	size_t len = tk->tk_len;

	if ((len == 4 && token_memcmp(tk, "warn", 4) == 0) ||
	    (len == 5 && token_memcmp(tk, "warnx", 5) == 0) ||
	    (len == 5 && token_memcmp(tk, "vwarn", 5) == 0) ||
	    (len == 6 && token_memcmp(tk, "vwarnx", 6) == 0) ||
	    (len == 6 && token_memcmp(tk, "perror", 6) == 0)) {
		*out = FORMAT_ARGNO_0;
		return 1;
	} else if ((len == 3 && token_memcmp(tk, "err", 3) == 0) ||
	    (len == 4 && token_memcmp(tk, "errx", 4) == 0) ||
	    (len == 4 && token_memcmp(tk, "verr", 4) == 0) ||
	    (len == 5 && token_memcmp(tk, "verrx", 5) == 0) ||
	    (len == 5 && token_memcmp(tk, "warnc", 5) == 0) ||
	    (len == 6 && token_memcmp(tk, "vwarnc", 6) == 0)) {
		*out = FORMAT_ARGNO_1;
		return 1;
	} else if ((len == 4 && token_memcmp(tk, "errc", 4) == 0) ||
	    (len == 5 && token_memcmp(tk, "verrc", 5) == 0)) {
		*out = FORMAT_ARGNO_2;
		return 1;
	}
	return 0;
}

static struct token *
find_format_argument_inner(struct lexer *lx, enum format_argno argno)
{
	struct token *format = NULL;
	int i;

	if (!lexer_if(lx, TOKEN_IDENT, NULL) ||
	    !lexer_if(lx, TOKEN_LPAREN, NULL))
		return NULL;

	for (i = 0; i < (int)argno; i++) {
		struct token *ignore;

		if (!lexer_pop(lx, &ignore) || !lexer_if(lx, TOKEN_COMMA, NULL))
			return NULL;
	}
	while (lexer_if(lx, TOKEN_STRING, &format))
		continue;
	return format;
}

static struct token *
find_format_argument(struct lexer *lx, struct token *seek,
    enum format_argno argno)
{
	struct lexer_state ls;
	struct token *format;

	lexer_peek_enter(lx, &ls);
	lexer_seek(lx, seek);
	format = find_format_argument_inner(lx, argno);
	lexer_peek_leave(lx, &ls);
	return format;
}

static int
string_has_line(const char *str, size_t len)
{
	const char suffix[] = "\\n\"";
	size_t suffixlen = sizeof(suffix) - 1;

	return len >= suffixlen &&
	    strncmp(&str[len - suffixlen], suffix, suffixlen) == 0;
}

void
simple_expr_printf(struct lexer *lx, struct token *tk, struct arena_scope *s)
{
	enum format_argno argno;
	const char *sanitized_format;
	struct token *format;

	if (!identify_function(tk, &argno))
		return;
	format = find_format_argument(lx, tk, argno);
	if (format == NULL)
		return;
	if (!string_has_line(format->tk_str, format->tk_len))
		return;

	sanitized_format = arena_sprintf(s, "\"%.*s\"",
	    (int)(format->tk_len - 1 /* " */ - 3 /* \n" */),
	    &format->tk_str[1]);
	token_set_str(format, sanitized_format, strlen(sanitized_format));
}
