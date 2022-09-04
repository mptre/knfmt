#include "comment.h"

#include <stdlib.h>
#include <string.h>

#include "extern.h"

static const char	*nextline(const char *, size_t);
static const char	*skipws(const char *, size_t);

char *
comment_exec(const struct token *tk, const struct config *UNUSED(cf))
{
	struct buffer *bf;
	const char *sp = tk->tk_str;
	char *p;
	size_t len = tk->tk_len;

	if (len == 0 || sp[len - 1] != '\n')
		return NULL;

	bf = buffer_alloc(len);
	for (;;) {
		const char *ep;

		ep = skipws(sp, len);
		if (ep != NULL && (*ep == '*' || *ep == '/')) {
			buffer_indent(bf, strwidth(sp, ep - sp, 0), 0);
			len -= ep - sp;
			sp += ep - sp;
		}
		ep = nextline(sp, len);
		if (ep == NULL)
			break;
		buffer_append(bf, sp, ep - sp);

		len -= ep - sp;
		sp += ep - sp;
	}
	buffer_appendc(bf, '\0');

	p = buffer_release(bf);
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
