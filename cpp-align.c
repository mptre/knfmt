#include "cpp-align.h"

#include "config.h"

#include <ctype.h>
#include <err.h>
#include <string.h>

#include "buffer.h"
#include "doc.h"
#include "options.h"
#include "ruler.h"
#include "style.h"
#include "token.h"
#include "util.h"

#define cpp_trace(op, fmt, ...) do {					\
	if (trace((op), 'C'))						\
		tracef('C', __func__, (fmt), __VA_ARGS__);		\
} while (0)

struct alignment {
	const char	*indent;
	unsigned int	 column;
	unsigned int	 tabs:1,
			 skip_first_line:1;
};

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

static int
alignment_cmp(const struct alignment *a, const struct alignment *b)
{
	return a->column == b->column && a->tabs == b->tabs ? 0 : 1;
}

static int
sense_alignment(const char *str, size_t len, const struct style *st,
    struct alignment *alignment)
{
	struct alignment lines[3] = {0};
	size_t nlines = sizeof(lines) / sizeof(lines[0]);
	size_t i;
	unsigned int maxcol = style(st, ColumnLimit);

	for (i = 0; i < nlines; i++) {
		const char *indent, *nx;
		size_t linelen;
		unsigned int col;

		indent = nextline(str, len, &nx);
		if (indent == NULL)
			return 0;

		linelen = (size_t)(nx - str);
		/* Require trailing '\\' '\n' characters. */
		if (linelen < 2)
			return 0;
		col = colwidth(str, linelen - 1, 1, NULL);
		if (col > maxcol)
			return 0;
		lines[i].indent = indent;
		lines[i].column = col - 2;
		lines[i].tabs = indent[0] == '\t';
		/* First line validated below. */
		if (i > 1 && alignment_cmp(&lines[i - 1], &lines[i]) != 0)
			return 0;

		len -= linelen;
		str += linelen;
	}
	if (alignment_cmp(&lines[0], &lines[1]) != 0) {
		/*
		 * If the first line is not aligned, only allow a single space
		 * before the line continuation.
		 */
		if (strncmp(lines[0].indent, " \\", 2) != 0)
			return 0;
		lines[nlines - 1].skip_first_line = 1;
	}

	*alignment = lines[nlines - 1];
	return 1;
}

/*
 * Align line continuations.
 */
char *
cpp_align(struct token *tk, const struct style *st, const struct options *op)
{
	struct ruler rl;
	struct alignment alignment = {0};
	struct buffer *bf;
	struct doc *dc;
	const char *nx, *str;
	char *p = NULL;
	size_t len;
	unsigned int align = style(st, AlignEscapedNewlines);
	unsigned int usetab = style(st, UseTab) != Never;
	int nlines = 0;

	str = tk->tk_str;
	len = tk->tk_len;
	if (nextline(str, len, &nx) == NULL)
		return NULL;

	if (sense_alignment(str, len, st, &alignment)) {
		cpp_trace(op, "column %u, tabs %d, skip %d",
		    alignment.column, alignment.tabs,
		    alignment.skip_first_line);
		ruler_init(&rl, (unsigned int)alignment.column,
		    RULER_ALIGN_MAX | (alignment.tabs ? RULER_ALIGN_TABS : 0));
	} else if (align == DontAlign) {
		ruler_init(&rl, 1, RULER_ALIGN_FIXED);
	} else if (align == Left) {
		ruler_init(&rl, 0, usetab ? RULER_ALIGN_TABS : RULER_ALIGN_MIN);
	} else if (align == Right) {
		ruler_init(&rl, style(st, ColumnLimit) - style(st, IndentWidth),
		    RULER_ALIGN_MAX | (usetab ? RULER_ALIGN_TABS : 0));
	} else {
		return NULL;
	}

	bf = buffer_alloc(len);
	if (bf == NULL)
		err(1, NULL);
	dc = doc_alloc(DOC_CONCAT, NULL);

	for (;;) {
		struct doc *concat;
		const char *ep, *sp;
		size_t cpplen, linelen;
		unsigned int w;

		concat = doc_alloc(DOC_CONCAT, dc);

		sp = str;
		ep = nextline(sp, len, &nx);
		if (ep == NULL)
			break;

		cpplen = (size_t)(ep - sp);
		if (cpplen > 0)
			doc_literal_n(sp, cpplen, concat);
		w = doc_width(&(struct doc_exec_arg){
		    .dc	= concat,
		    .bf	= bf,
		    .st	= st,
		    .op	= op,
		});
		if (nlines == 0 && alignment.skip_first_line)
			doc_literal(" ", concat);
		else
			ruler_insert(&rl, tk, concat, 1, w, 0);
		doc_literal("\\", concat);
		doc_alloc(DOC_HARDLINE, concat);

		linelen = (size_t)(nx - str);
		len -= linelen;
		str += linelen;
		nlines++;
	}
	if (len > 0)
		doc_literal_n(str, len, dc);

	/* Alignment only wanted for multiple lines. */
	if (nlines <= 1)
		goto out;
	ruler_exec(&rl);
	doc_exec(&(struct doc_exec_arg){
	    .dc	= dc,
	    .bf	= bf,
	    .st	= st,
	    .op	= op,
	});

	p = buffer_str(bf);
out:
	ruler_free(&rl);
	doc_free(dc);
	buffer_free(bf);
	return p;
}
