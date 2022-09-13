#include "style.h"

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "buffer.h"
#include "cdefs.h"
#include "error.h"
#include "lexer.h"
#include "options.h"
#include "token.h"
#include "util.h"

#ifdef HAVE_UTHASH
#  include <uthash.h>
#else
#  include "compat-uthash.h"
#endif

/* Continuation of token types used to represent YAML primitives. */
enum yaml_type {
	DocumentBegin = Last + 1,
	DocumentEnd,
	Sequence,
	String,
	False,
	True,
	Integer,
	Unknown,
};

struct style {
	unsigned int	st_options[Last];
};

struct style_hash {
	const char	*sh_key;
	int		 sh_type;
	UT_hash_handle	 hh;
};

static void	style_defaults(struct style *);
static int	style_parse_yaml(struct style *, const char *,
    const struct buffer *, const struct options *);
static int	style_parse_yaml1(struct style *, struct lexer *,
    const struct options *);
static int	style_parse_yaml_nested(struct style *, struct lexer *);

static struct token	*yaml_read(struct lexer *, void *);
static char		*yaml_serialize(const struct token *);
static struct token	*yaml_keyword(struct lexer *,
    const struct lexer_state *);

static const char	*stryaml(enum yaml_type);

static struct style_hash *keywords = NULL;

void
style_init(void)
{
	static struct style_hash kw[] = {
#define K(t, s)	{ .sh_key = s, .sh_type = t, }
		K(Align,				"Align"),
		K(AlignAfterOpenBracket,		"AlignAfterOpenBracket"),
		K(AlignEscapedNewlines,			"AlignEscapedNewlines"),
		K(AlignWithSpaces,			"AlignWithSpaces"),
		K(All,					"All"),
		K(AllDefinitions,			"AllDefinitions"),
		K(Always,				"Always"),
		K(AlwaysBreak,				"AlwaysBreak"),
		K(AlwaysBreakAfterReturnType,		"AlwaysBreakAfterReturnType"),
		K(BlockIndent,				"BlockIndent"),
		K(BraceWrapping,			"BraceWrapping"),
		K(ColumnLimit,				"ColumnLimit"),
		K(ContinuationIndentWidth,		"ContinuationIndentWidth"),
		K(DontAlign,				"DontAlign"),
		K(ForContinuationAndIndentation,	"ForContinuationAndIndentation"),
		K(ForIndentation,			"ForIndentation"),
		K(IncludeCategories,			"IncludeCategories"),
		K(IndentWidth,				"IndentWidth"),
		K(Left,					"Left"),
		K(Never,				"Never"),
		K(None,					"None"),
		K(Right,				"Right"),
		K(TopLevel,				"TopLevel"),
		K(TopLevelDefinitions,			"TopLevelDefinitions"),
		K(UseTab,				"UseTab"),
		/* YAML primitives. */
		K(DocumentBegin,			"---"),
		K(DocumentEnd,				"..."),
		K(Sequence,				"-"),
		K(False,				"false"),
		K(True,					"true"),
		K(Integer,				"Integer"),
#undef K
	};
	size_t nkeywords = sizeof(kw) / sizeof(kw[0]);
	size_t i;

	for (i = 0; i < nkeywords; i++)
		HASH_ADD_STR(keywords, sh_key, &kw[i]);
}

void
style_teardown(void)
{
	HASH_CLEAR(hh, keywords);
}

struct style *
style_parse(const struct options *op)
{
	static const char *filename = ".clang-format";
	struct style *st;
	int error = 0;
	int fd;

	st = calloc(1, sizeof(*st));
	if (st == NULL)
		err(1, NULL);
	style_defaults(st);

	fd = searchpath(filename);
	if (fd != -1) {
		struct buffer *bf;

		bf = buffer_read_fd(fd);
		close(fd);
		error = style_parse_yaml(st, filename, bf, op);
		buffer_free(bf);
	}
	if (error) {
		style_free(st);
		st = NULL;
	}

	return st;
}

void
style_free(struct style *st)
{
	if (st == NULL)
		return;
	free(st);
}

unsigned int
style(const struct style *st, int option)
{
	assert(option < Last);
	return st->st_options[option];
}

static void
style_defaults(struct style *st)
{
	st->st_options[AlignAfterOpenBracket] = DontAlign;
	st->st_options[AlignEscapedNewlines] = Right;
	st->st_options[AlwaysBreakAfterReturnType] = AllDefinitions;
	st->st_options[ColumnLimit] = 80;
	st->st_options[ContinuationIndentWidth] = 4;
	st->st_options[IndentWidth] = 8;
	st->st_options[UseTab] = Always;
}

static int
style_parse_yaml(struct style *st, const char *path, const struct buffer *bf,
    const struct options *op)
{
	struct error *er;
	struct lexer *lx;
	int error = 0;

	er = error_alloc(1);
	lx = lexer_alloc(&(const struct lexer_arg){
		.path		= path,
		.bf		= bf,
		.er		= er,
		.diff		= NULL,
		.op		= op,
		.callbacks	= {
			.read		= yaml_read,
			.serialize	= yaml_serialize,
			.arg		= st
		},
	});
	if (lx == NULL) {
		error = 1;
		goto out;
	}
	error = style_parse_yaml1(st, lx, op);

out:
	lexer_free(lx);
	error_free(er);
	return error;
}

static int
style_parse_yaml1(struct style *st, struct lexer *lx, const struct options *op)
{
	lexer_if(lx, DocumentBegin, NULL);

	for (;;) {
		struct token *key = NULL;
		struct token *val = NULL;

		if (lexer_if(lx, LEXER_EOF, NULL))
			break;

		if (lexer_if(lx, AlignEscapedNewlines, &key) &&
		    (lexer_if(lx, Align, &val) ||
		     lexer_if(lx, DontAlign, &val) ||
		     lexer_if(lx, Left, &val) ||
		     lexer_if(lx, Right, &val))) {
			st->st_options[key->tk_type] = val->tk_type;
		} else if (lexer_if(lx, AlignAfterOpenBracket, &key) &&
		    (lexer_if(lx, Align, &val) ||
		     lexer_if(lx, DontAlign, &val) ||
		     lexer_if(lx, AlwaysBreak, &val) ||
		     lexer_if(lx, BlockIndent, &val))) {
			st->st_options[key->tk_type] = val->tk_type;
		} else if (lexer_if(lx, AlwaysBreakAfterReturnType, &key) &&
		    (lexer_if(lx, None, &val) ||
		     lexer_if(lx, All, &val) ||
		     lexer_if(lx, TopLevel, &val) ||
		     lexer_if(lx, AllDefinitions, &val) ||
		     lexer_if(lx, TopLevelDefinitions, &val))) {
			st->st_options[key->tk_type] = val->tk_type;
		} else if (lexer_if(lx, ColumnLimit, &key) &&
		    lexer_if(lx, Integer, &val)) {
			st->st_options[key->tk_type] = val->tk_int;
		} else if (lexer_if(lx, ContinuationIndentWidth, &key) &&
		    lexer_if(lx, Integer, &val)) {
			st->st_options[key->tk_type] = val->tk_int;
		} else if (lexer_if(lx, IndentWidth, &key) &&
		    lexer_if(lx, Integer, &val)) {
			st->st_options[key->tk_type] = val->tk_int;
		} else if (lexer_if(lx, UseTab, &key) &&
		    (lexer_if(lx, Never, &val) ||
		     lexer_if(lx, ForIndentation, &val) ||
		     lexer_if(lx, ForContinuationAndIndentation, &val) ||
		     lexer_if(lx, AlignWithSpaces, &val) ||
		     lexer_if(lx, Always, &val))) {
			st->st_options[key->tk_type] = val->tk_type;
		} else if (lexer_if(lx, BraceWrapping, &key) ||
		    lexer_if(lx, IncludeCategories, &key)) {
			style_parse_yaml_nested(st, lx);
			if (trace(op, 's') >= 2) {
				char *strkey;

				strkey = yaml_serialize(key);
				lexer_error(lx, "skip nested option %s",
				    strkey);
				free(strkey);
			}
		} else if (lexer_if(lx, DocumentEnd, NULL)) {
			/* nothing */
		} else if (key != NULL) {
			lexer_pop(lx, &val);
			if (trace(op, 's') >= 2) {
				char *strkey, *strval;

				strkey = yaml_serialize(key);
				strval = val != NULL ?
				    yaml_serialize(val) : NULL;
				lexer_error(lx,
				    "unknown value %s for option %s",
				    strval != NULL ? strval : "(null)", strkey);
				free(strkey);
				free(strval);
			}
		} else {
			/* Best effort, try to continue parsing. */
			lexer_pop(lx, &key);
			if (lexer_peek_if(lx, Sequence, NULL)) {
				/* Ignore sequences. */
				while (lexer_if(lx, Sequence, NULL))
					lexer_pop(lx, &val);
			} else {
				lexer_pop(lx, &val);
			}
			if (trace(op, 's') >= 2) {
				char *strkey;

				strkey = key != NULL
				    ? yaml_serialize(key) : NULL;
				lexer_error(lx, "unknown option %s",
				    strkey != NULL ? strkey : "(null)");
				free(strkey);
			}
		}
	}

	return trace(op, 's') >= 2 ? lexer_get_error(lx) : 0;
}

static int
style_parse_yaml_nested(struct style *UNUSED(st), struct lexer *lx)
{
	struct token *key, *val;

	while (lexer_peek(lx, &key) && token_has_indent(key) &&
	    lexer_pop(lx, &key) && lexer_pop(lx, &val))
		continue;
	return 0;
}

static struct token *
yaml_read(struct lexer *lx, void *UNUSED(arg))
{
	struct lexer_state s;
	struct token *tk;
	unsigned char ch;

again:
	do {
		if (lexer_getc(lx, &ch))
			goto eof;
	} while (isspace(ch));
	lexer_ungetc(lx);

	lexer_get_state(lx, &s);

	if (lexer_getc(lx, &ch))
		goto eof;

	/*
	 * Cheat a bit by discarding certain expected tokens. Makes the parser
	 * less strict but also less repetitive.
	 */
	if (ch == ':')
		goto again;

	if (ch == '#') {
		for (;;) {
			if (lexer_getc(lx, &ch))
				goto eof;
			if (ch == '\n')
				break;
		}
		goto again;
	}

	if (isalpha(ch)) {
		do {
			if (lexer_getc(lx, &ch))
				goto eof;
		} while (isalpha(ch));
		lexer_ungetc(lx);
		return yaml_keyword(lx, &s);
	}

	if (isdigit(ch)) {
		int overflow = 0;
		int digit = 0;

		while (isdigit(ch)) {
			int x = ch - '0';

			if (digit > INT_MAX / 10)
				overflow = 1;
			else
				digit *= 10;

			if (digit > INT_MAX - x)
				overflow = 1;
			else
				digit += x;

			if (lexer_getc(lx, &ch))
				goto eof;
		}
		lexer_ungetc(lx);

		tk = lexer_emit(lx, &s, NULL);
		tk->tk_type = Integer;
		tk->tk_int = digit;
		if (overflow) {
			char *str;

			str = yaml_serialize(tk);
			lexer_error(lx, "integer %s too large", str);
			free(str);
		}
		return tk;
	}

	if (ch == '-' || ch == '.') {
		unsigned char needle = ch;

		do {
			if (lexer_getc(lx, &ch))
				goto eof;
		} while (ch == needle);
		lexer_ungetc(lx);
		return yaml_keyword(lx, &s);
	}

	if (ch == '\'') {
		for (;;) {
			if (lexer_getc(lx, &ch))
				goto eof;
			if (ch == '\'')
				break;
		}
		tk = lexer_emit(lx, &s, NULL);
		tk->tk_type = String;
		return tk;
	}

	tk = lexer_emit(lx, &s, NULL);
	tk->tk_type = Unknown;
	return tk;

eof:
	tk = lexer_emit(lx, &s, NULL);
	tk->tk_type = LEXER_EOF;
	return tk;
}

static char *
yaml_serialize(const struct token *tk)
{
	char *val = NULL;
	char *buf;
	ssize_t bufsiz = 256;
	int n;

	buf = malloc(bufsiz);
	if (tk->tk_type < Last) {
		val = strnice(tk->tk_str, tk->tk_len);
		n = snprintf(buf, bufsiz, "Keyword<%u:%u>(\"%s\")",
		    tk->tk_lno, tk->tk_cno, val);
	} else {
		val = strnice(tk->tk_str, tk->tk_len);
		n = snprintf(buf, bufsiz, "%s<%u:%u>(\"%s\")",
		    stryaml(tk->tk_type), tk->tk_lno, tk->tk_cno, val);
	}
	if (n < 0)
		err(1, "asprintf");
	free(val);
	return buf;
}

static struct token *
yaml_keyword(struct lexer *lx, const struct lexer_state *st)
{
	const struct style_hash *sh;
	struct token *tk;

	tk = lexer_emit(lx, st, NULL);
	HASH_FIND(hh, keywords, tk->tk_str, tk->tk_len, sh);
	if (sh == NULL) {
		tk->tk_type = Unknown;
		return tk;
	}
	tk->tk_type = sh->sh_type;
	return tk;
}

static const char *
stryaml(enum yaml_type type)
{
	switch (type) {
#define CASE(t) case t: return #t; break
	CASE(DocumentBegin);
	CASE(DocumentEnd);
	CASE(Sequence);
	CASE(String);
	CASE(False);
	CASE(True);
	CASE(Integer);
	CASE(Unknown);
#undef CASE
	}
	if (type == LEXER_EOF)
		return "EOF";
	return NULL;
}
