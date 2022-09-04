#include "cpp.h"

#include <ctype.h>
#include <string.h>

#include "extern.h"
#include "ruler.h"

static const char	*nextline(const char *, size_t, const char **);

/*
 * Formatting of preprocessor tokens. Currently limited to alignment of line
 * continuations.
 */
char *
cpp_exec(const struct token *tk, const struct config *cf)
{
	struct ruler rl;
	struct buffer *bf;
	struct doc *dc;
	const char *nx, *str;
	char *p;
	size_t len;
	int nlines = 0;

	str = tk->tk_str;
	len = tk->tk_len;
	if (nextline(str, len, &nx) == NULL)
		return NULL;

	bf = buffer_alloc(len);
	dc = doc_alloc(DOC_CONCAT, NULL);
	ruler_init(&rl, cf->cf_mw - cf->cf_tw);

	for (;;) {
		struct doc *concat;
		const char *ep, *sp;

		concat = doc_alloc(DOC_CONCAT, dc);

		sp = str;
		ep = nextline(sp, len, &nx);
		if (ep == NULL)
			break;

		if (nlines > 0)
			doc_alloc(DOC_HARDLINE, concat);
		if (ep - sp > 0)
			doc_literal_n(sp, ep - sp, concat);
		ruler_insert(&rl, tk, concat, 1, doc_width(concat, bf, cf), 0);
		doc_literal("\\", concat);

		len -= nx - str;
		str += nx - str;
		nlines++;
	}
	if (len > 0) {
		if (nlines > 0)
			doc_alloc(DOC_HARDLINE, dc);
		doc_literal_n(str, len, dc);
	}

	/* Alignment only wanted for multiple lines. */
	if (nlines > 1)
		ruler_exec(&rl);
	doc_exec(dc, NULL, bf, cf, 0);
	buffer_appendc(bf, '\0');

	p = buffer_release(bf);
	ruler_free(&rl);
	doc_free(dc);
	buffer_free(bf);
	return p;
}

/*
 * Returns a pointer to the end of current line assuming it's a line
 * continuation.
 */
static const char *
nextline(const char *str, size_t len, const char **nx)
{
	const char *p;

	p = memchr(str, '\n', len);
	if (p == NULL || p == str || p[-1] != '\\')
		return NULL;
	*nx = &p[1];
	p--;	/* consume '\\' */
	while (p > str && isspace((unsigned char)p[-1]))
		p--;
	return p;
}
