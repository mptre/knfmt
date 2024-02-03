#include "comment.h"

#include "config.h"

#include <ctype.h>
#include <err.h>
#include <string.h>

#include "libks/arena-buffer.h"
#include "libks/buffer.h"

#include "style.h"
#include "token.h"
#include "util.h"

static const char	*nextline(const char *, size_t);
static const char	*skipws(const char *, size_t);
static size_t		 rskipws(const char *, size_t);

struct buffer *
comment_trim(const struct token *tk, const struct style *st,
    struct arena_scope *s)
{
	struct buffer *bf;
	const char *sp = tk->tk_str;
	size_t len = tk->tk_len;
	int iscrlf;

	if (len == 0 || sp[len - 1] != '\n')
		return NULL;

	iscrlf = len >= 2 && sp[len - 2] == '\r';
	bf = arena_buffer_alloc(s, len);
	for (;;) {
		const char *ep;
		size_t commlen;

		ep = skipws(sp, len);
		if (ep != NULL && (*ep == '*' || *ep == '/')) {
			size_t wslen;

			wslen = (size_t)(ep - sp);
			strindent_buffer(bf, strwidth(sp, wslen, 0),
			    style_use_tabs(st), 0);
			len -= wslen;
			sp += wslen;
		}
		ep = nextline(sp, len);
		if (ep == NULL)
			break;
		commlen = (size_t)(ep - sp);
		buffer_puts(bf, sp, rskipws(sp, commlen));
		if (iscrlf)
			buffer_putc(bf, '\r');
		buffer_putc(bf, '\n');

		len -= commlen;
		sp += commlen;
	}

	return bf;
}

static const char *
nextline(const char *str, size_t len)
{
	const char *p;

	p = memchr(str, '\n', len);
	if (p == NULL)
		return NULL;
	return &p[1];
}

static const char *
skipws(const char *str, size_t len)
{
	size_t i = 0;

	for (; len > 0 && (str[i] == ' ' || str[i] == '\t'); i++, len--)
		continue;
	if (i == 0)
		return NULL;
	return &str[i];
}

/*
 * Returns the length of the given string disregarding trailing whitespace.
 */
static size_t
rskipws(const char *str, size_t len)
{
	while (len > 0 && isspace((unsigned char)str[len - 1]))
		len--;
	return len;
}
