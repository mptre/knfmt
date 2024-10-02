#include "cpp-align.h"

#include "config.h"

#include <ctype.h>
#include <string.h>

#include "libks/arena-buffer.h"
#include "libks/arena.h"
#include "libks/buffer.h"

#include "doc.h"
#include "ruler.h"
#include "style.h"
#include "token.h"
#include "trace-types.h"
#include "trace.h"
#include "util.h"

#define cpp_trace(op, fmt, ...) trace(TRACE_CPP, (op), (fmt), __VA_ARGS__)

enum indent_type {
	INDENT_TYPE_NONE,
	INDENT_TYPE_SPACES,
	INDENT_TYPE_TABS,
};

struct alignment {
	const char		*indent;
	enum indent_type	 indent_type;
	enum style_keyword	 mode;
	unsigned int		 width;
	unsigned int		 tabs:1,
				 skip_first_line:1;
};

static enum indent_type
classify_indent(const char *str)
{
	if (str[0] == '\t')
		return INDENT_TYPE_TABS;
	if (str[0] == ' ')
		return INDENT_TYPE_SPACES;
	return INDENT_TYPE_NONE;
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

static int
is_not_aligned(const struct alignment *a)
{
	return strncmp(a->indent, " \\", 2) == 0;
}

static int
all_identical(const struct alignment *a, unsigned int len)
{
	unsigned int i;

	for (i = 0; i < len - 1; i++) {
		if (a[i].width != a[i + 1].width)
			return 0;

		if (a[i].indent_type == INDENT_TYPE_NONE ||
		    a[i + 1].indent_type == INDENT_TYPE_NONE)
			continue;
		if (a[i].indent_type != a[i + 1].indent_type)
			return 0;
	}
	return 1;
}

static int
all_not_aligned(const struct alignment *a, unsigned int len)
{
	unsigned int i;

	for (i = 0; i < len; i++) {
		if (!is_not_aligned(&a[i]))
			return 0;
	}
	return 1;
}

static int
all_tabs(const struct alignment *a, unsigned int len)
{
	unsigned int i;

	for (i = 0; i < len; i++) {
		if (a[i].indent_type != INDENT_TYPE_NONE &&
		    a[i].indent_type != INDENT_TYPE_TABS)
			return 0;
	}
	return 1;
}

static int
sense_alignment(const char *str, size_t len, const struct style *st,
    struct alignment *alignment)
{
	struct alignment lines[3] = {0};
	unsigned int maxcol = style(st, ColumnLimit);
	unsigned int nlines = 0;
	unsigned int i;

	for (i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
		const char *indent, *nx;
		size_t linelen;
		unsigned int col;

		indent = nextline(str, len, &nx);
		if (indent == NULL)
			break;

		linelen = (size_t)(nx - str);
		/* Require trailing '\\' '\n' characters. */
		if (linelen < 2)
			return 0;
		col = colwidth(str, linelen - 1, 1);
		if (col > maxcol)
			return 0;
		lines[i].indent = indent;
		lines[i].indent_type = classify_indent(indent);
		lines[i].width = col - 2;
		len -= linelen;
		str += linelen;
		nlines++;
	}

	if (all_not_aligned(lines, nlines)) {
		*alignment = (struct alignment){.mode = DontAlign};
		return 1;
	}

	/* The first line is allowed to not be aligned. */
	if (nlines >= 3 && all_identical(&lines[1], nlines - 1)) {
		*alignment = (struct alignment){
		    .mode		= Right,
		    .width		= lines[nlines - 1].width,
		    .tabs		= all_tabs(lines, nlines) ? 1 : 0,
		    .skip_first_line	= is_not_aligned(&lines[0]) ? 1 : 0,
		};
		return 1;
	}
	return 0;
}

/*
 * Align line continuations.
 */
const char *
cpp_align(struct token *tk, const struct style *st, struct arena_scope *s,
    struct arena *scratch, const struct options *op)
{
	struct alignment alignment = {
		.mode	= style(st, AlignEscapedNewlines),
		.width	= style(st, ColumnLimit) - style(st, IndentWidth),
		.tabs	= style_use_tabs(st) ? 1 : 0,
	};
	struct ruler rl;
	struct buffer *bf;
	struct doc *dc;
	const char *out = NULL;
	const char *nx, *str;
	size_t len;
	int nlines = 0;

	str = tk->tk_str;
	len = tk->tk_len;
	if (nextline(str, len, &nx) == NULL)
		return NULL;

	if (sense_alignment(str, len, st, &alignment)) {
		cpp_trace(op, "mode %s, width %u, tabs %d, skip %d",
		    style_keyword_str(alignment.mode), alignment.width,
		    alignment.tabs ? 1 : 0,
		    alignment.skip_first_line ? 1 : 0);
	}
	switch (alignment.mode) {
	case DontAlign:
		ruler_init(&rl, 1, RULER_ALIGN_FIXED);
		break;
	case Left:
		ruler_init(&rl, 0,
		    alignment.tabs ? RULER_ALIGN_TABS : RULER_ALIGN_MIN);
		break;
	case Right:
		ruler_init(&rl, alignment.width,
		    RULER_ALIGN_MAX | (alignment.tabs ? RULER_ALIGN_TABS : 0));
		break;
	default:
		return NULL;
	}

	arena_scope(scratch, scratch_scope);

	bf = arena_buffer_alloc(s, len);
	dc = doc_root(&scratch_scope);

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
		if (cpplen > 0) {
			const char *literal;

			literal = arena_strndup(&scratch_scope, sp, cpplen);
			doc_literal(literal, concat);
		}
		buffer_reset(bf);
		w = doc_width(&(struct doc_exec_arg){
		    .dc		= concat,
		    .scratch	= scratch,
		    .bf		= bf,
		    .st		= st,
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
	if (len > 0) {
		const char *literal;

		literal = arena_strndup(&scratch_scope, str, len);
		doc_literal(literal, dc);
	}

	/* Alignment only wanted for multiple lines. */
	if (nlines <= 1)
		goto out;
	ruler_exec(&rl);
	buffer_reset(bf);
	doc_exec(&(struct doc_exec_arg){
	    .dc		= dc,
	    .scratch	= scratch,
	    .bf		= bf,
	    .st		= st,
	});
	out = buffer_str(bf);

out:
	ruler_free(&rl);
	return out;
}
