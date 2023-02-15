#include "comment.h"

#include "config.h"

#include <ctype.h>
#include <err.h>
#include <string.h>

#include "buffer.h"
#include "cdefs.h"
#include "style.h"
#include "token.h"
#include "util.h"

static const char	*nextline(const char *, size_t);
static const char	*skipws(const char *, size_t);
static size_t		 rskipws(const char *, size_t);

char *
comment_trim(const struct token *tk, const struct style *st,
    const struct options *UNUSED(op))
{
	struct buffer *bf;
	const char *sp = tk->tk_str;
	char *p;
	size_t len = tk->tk_len;
	int iscrlf;

	if (len == 0 || sp[len - 1] != '\n')
		return NULL;

	iscrlf = len >= 2 && sp[len - 2] == '\r' && sp[len - 1] == '\n';
	bf = buffer_alloc(len);
	if (bf == NULL)
		err(1, NULL);
	for (;;) {
		const char *ep;
		size_t commlen;

		ep = skipws(sp, len);
		if (ep != NULL && (*ep == '*' || *ep == '/')) {
			size_t wslen;

			wslen = (size_t)(ep - sp);
			strindent_buffer(bf, strwidth(sp, wslen, 0),
			    style(st, UseTab) != Never, 0);
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

	p = buffer_str(bf);
	buffer_free(bf);
	return p;
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
