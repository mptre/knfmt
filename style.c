#include "style.h"

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libks/arithmetic.h"
#include "libks/buffer.h"
#include "libks/vector.h"

#include "alloc.h"
#include "cdefs.h"
#include "error.h"
#include "lexer.h"
#include "options.h"
#include "token.h"
#include "util.h"

/*
 * Return values for yaml parser routines. Only one of the following may be
 * returned but disjoint values are favored allowing the caller to catch
 * multiple return values.
 */
#define NONE	0x00000000
#define GOOD	0x00000001
#define SKIP	0x00000002
#define FAIL	0x00000004

#define FOR_YAML_TYPES(OP)						\
	OP(DocumentBegin,	1)					\
	OP(DocumentEnd,		2)					\
	OP(Colon,		3)					\
	OP(Sequence,		4)					\
	OP(String,		5)					\
	OP(Integer,		6)					\
	OP(Unknown,		7)					\

/* Continuation of token types used to represent YAML primitives. */
enum yaml_type {
#define OP(type, idx) type = Last + (idx),
	FOR_YAML_TYPES(OP)
#undef OP
};

struct style {
	const struct options	*st_op;
	int			 st_scope;
	struct {
		int		type;
		unsigned int	isset;
		unsigned int	val;
	} st_options[Last];
};

struct style_option {
	int		 so_scope;
	const char	*so_key;
	size_t		 so_len;
	int		 so_type;
	int		 (*so_parse)(struct style *, struct lexer *,
	    const struct style_option *);
	int		 so_val[16];
};

static void	style_defaults(struct style *);
static void	style_set(struct style *, int, int, unsigned int);
static int	style_parse_yaml(struct style *, const char *,
    const struct buffer *, const struct options *);
static int	style_parse_yaml1(struct style *, struct lexer *);
static void	style_dump(const struct style *);

static struct token			*yaml_read(struct lexer *, void *);
static struct token			*yaml_read_integer(struct lexer *);
static char				*yaml_serialize(const struct token *);
static struct token			*yaml_keyword(struct lexer *,
    const struct lexer_state *);
static const struct style_option	*yaml_find_keyword(const char *,
    size_t);

static int	parse_bool(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_enum(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_integer(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_string(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_nested(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_AlignOperands(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_ColumnLimit(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_IncludeCategories(struct style *, struct lexer *,
    const struct style_option *);

static const char	*yaml_type_str(enum yaml_type);

static struct style_option *keywords[256];

void
style_init(void)
{
	static struct style_option kw[] = {
/* Top level style option. */
#define S(t)	0,	#t,	sizeof(#t) - 1,	(t)
/* Nested style option. */
#define N(s, t)	(s),	#t,	sizeof(#t) - 1,	(t)
/* Enum style option value. */
#define E(t)	0,	#t,	sizeof(#t) - 1,	(t),	NULL,	{0}
/* YAML primitive. */
#define P(t, s)	0,	(s),	sizeof(s) - 1,	(t),	NULL,	{0}

		{ S(AlignAfterOpenBracket), parse_enum,
		  { Align, DontAlign, AlwaysBreak, BlockIndent } },

		{ S(AlignEscapedNewlines), parse_enum,
		  { DontAlign, Left, Right } },

		{ S(AlignOperands), parse_AlignOperands,
		  { Align, DontAlign, AlignAfterOperator, True, False } },

		{ S(AlwaysBreakAfterReturnType), parse_enum,
		  { None, All, TopLevel, AllDefinitions, TopLevelDefinitions } },

		{ S(BitFieldColonSpacing), parse_enum,
		  { Both, None, Before, After } },

		{ S(BraceWrapping), parse_nested, {0} },
		{ N(BraceWrapping, AfterCaseLabel), parse_bool, {0} },
		{ N(BraceWrapping, AfterClass), parse_bool, {0} },
		{ N(BraceWrapping, AfterControlStatement), parse_enum,
		  { Never, MultiLine, Always, True, False } },
		{ N(BraceWrapping, AfterEnum), parse_bool, {0} },
		{ N(BraceWrapping, AfterExternBlock), parse_bool, {0} },
		{ N(BraceWrapping, AfterFunction), parse_bool, {0} },
		{ N(BraceWrapping, AfterNamespace), parse_bool, {0} },
		{ N(BraceWrapping, AfterObjCDeclaration), parse_bool, {0} },
		{ N(BraceWrapping, AfterStruct), parse_bool, {0} },
		{ N(BraceWrapping, AfterUnion), parse_bool, {0} },
		{ N(BraceWrapping, BeforeCatch), parse_bool, {0} },
		{ N(BraceWrapping, BeforeElse), parse_bool, {0} },
		{ N(BraceWrapping, BeforeLambdaBody), parse_bool, {0} },
		{ N(BraceWrapping, BeforeWhile), parse_bool, {0} },
		{ N(BraceWrapping, IndentBraces), parse_bool, {0} },
		{ N(BraceWrapping, SplitEmptyFunction), parse_bool, {0} },
		{ N(BraceWrapping, SplitEmptyNamespace), parse_bool, {0} },
		{ N(BraceWrapping, SplitEmptyRecord), parse_bool, {0} },

		{ S(BreakBeforeBinaryOperators), parse_enum,
		  { None, NonAssignment, All } },

		{ S(BreakBeforeBraces), parse_enum,
		  { Attach, Linux, Mozilla, Stroustrup, Allman, Whitesmiths,
		    GNU, WebKit, Custom } },

		{ S(BreakBeforeTernaryOperators), parse_bool, {0} },

		{ S(ColumnLimit), parse_ColumnLimit, {0} },

		{ S(ContinuationIndentWidth), parse_integer, {0} },

		{ S(IncludeCategories), parse_IncludeCategories, {0} },
		{ N(IncludeCategories, CaseSensitive), parse_bool, {0} },
		{ N(IncludeCategories, Priority), parse_integer, {0} },
		{ N(IncludeCategories, Regex), parse_string, {0} },
		{ N(IncludeCategories, SortPriority), parse_integer, {0} },

		{ S(IndentWidth), parse_integer, {0} },

		{ S(SortIncludes), parse_enum,
		  { Never, CaseSensitive, CaseInsensitive } },

		{ S(UseTab), parse_enum,
		  { Never, ForIndentation, ForContinuationAndIndentation,
		    AlignWithSpaces, Always } },

		/* enum */
		{ E(After) },
		{ E(Align) },
		{ E(AlignAfterOperator) },
		{ E(AlignWithSpaces) },
		{ E(All) },
		{ E(AllDefinitions) },
		{ E(Allman) },
		{ E(Always) },
		{ E(AlwaysBreak) },
		{ E(Attach) },
		{ E(Before) },
		{ E(BlockIndent) },
		{ E(Both) },
		{ E(Custom) },
		{ E(DontAlign) },
		{ E(ForContinuationAndIndentation) },
		{ E(ForIndentation) },
		{ E(GNU) },
		{ E(Left) },
		{ E(Linux) },
		{ E(Mozilla) },
		{ E(MultiLine) },
		{ E(Never) },
		{ E(NonAssignment) },
		{ E(None) },
		{ E(Right) },
		{ E(Stroustrup) },
		{ E(TopLevel) },
		{ E(TopLevelDefinitions) },
		{ E(WebKit) },
		{ E(Whitesmiths) },

		/* primitives */
		{ P(Colon, ":") },
		{ P(DocumentBegin, "---"), },
		{ P(DocumentEnd, "..."), },
		{ P(False, "false") },
		{ P(Integer, "Integer") },
		{ P(Sequence, "-") },
		{ P(True, "true") },

#undef Y
#undef S
	};
	size_t nkeywords = sizeof(kw) / sizeof(kw[0]);
	size_t i;

	for (i = 0; i < nkeywords; i++) {
		const struct style_option *src = &kw[i];
		struct style_option *dst;
		unsigned char slot;

		slot = (unsigned char)src->so_key[0];
		if (keywords[slot] == NULL) {
			if (VECTOR_INIT(keywords[slot]))
				err(1, NULL);
		}
		dst = VECTOR_ALLOC(keywords[slot]);
		if (dst == NULL)
			err(1, NULL);
		*dst = *src;
	}
}

void
style_shutdown(void)
{
	size_t nslots = sizeof(keywords) / sizeof(keywords[0]);
	size_t i;

	for (i = 0; i < nslots; i++)
		VECTOR_FREE(keywords[i]);
}

struct style *
style_parse(const char *path, const struct options *op)
{
	struct buffer *bf = NULL;
	struct style *st;

	if (path != NULL) {
		bf = buffer_read(path);
	} else {
		int fd;

		path = ".clang-format";
		fd = searchpath(path, NULL);
		if (fd != -1) {
			bf = buffer_read_fd(fd);
			close(fd);
		}
	}
	st = style_parse_buffer(bf, path, op);
	buffer_free(bf);
	if (st != NULL && trace(op, 's') >= 2)
		style_dump(st);
	return st;
}

struct style *
style_parse_buffer(const struct buffer *bf, const char *path,
    const struct options *op)
{
	struct style *st;

	st = ecalloc(1, sizeof(*st));
	st->st_op = op;
	style_defaults(st);
	if (bf != NULL && style_parse_yaml(st, path, bf, op)) {
		style_free(st);
		return NULL;
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
	return st->st_options[option].val;
}

int
style_brace_wrapping(const struct style *st, int option)
{
	switch (st->st_options[BreakBeforeBraces].val) {
	case Linux:
		switch (option) {
		case AfterFunction:
			return 1;
		}
		break;
	}
	return st->st_options[option].val == True;
}

static void
style_defaults(struct style *st)
{
	static const struct {
		unsigned int	key;
		unsigned int	val;
	} defaults[] = {
		{ AlignAfterOpenBracket,	DontAlign },
		{ AlignEscapedNewlines,		Right },
		{ AlignOperands,		DontAlign },
		{ AlwaysBreakAfterReturnType,	AllDefinitions },
		{ BitFieldColonSpacing,		None },
		{ BreakBeforeBinaryOperators,	None },
		{ BreakBeforeBraces,		Linux },
		{ BreakBeforeTernaryOperators,	False },
		{ ColumnLimit,			80 },
		{ ContinuationIndentWidth,	4 },
		{ IndentWidth,			8 },
		{ SortIncludes,			Never },
		{ UseTab,			Always },
	};
	size_t ndefaults = sizeof(defaults) / sizeof(defaults[0]);
	size_t i;

	for (i = 0; i < ndefaults; i++)
		st->st_options[defaults[i].key].val = defaults[i].val;
}

static void
style_set(struct style *st, int option, int type, unsigned int val)
{
	st->st_options[option].type = type;
	st->st_options[option].isset = 1;
	st->st_options[option].val = val;
}

static int
style_parse_yaml(struct style *st, const char *path, const struct buffer *bf,
    const struct options *op)
{
	struct error *er;
	struct lexer *lx;
	int error = 0;

	er = error_alloc(0);
	lx = lexer_alloc(&(const struct lexer_arg){
	    .path	= path,
	    .bf		= bf,
	    .er		= er,
	    .op		= op,
	    .callbacks	= {
		.read		= yaml_read,
		.serialize	= yaml_serialize,
		.arg		= st,
	    },
	});
	if (lx == NULL) {
		error = 1;
		goto out;
	}
	error = style_parse_yaml1(st, lx);

out:
	lexer_free(lx);
	error_flush(er, trace(op, 's') > 0);
	error_free(er);
	return error;
}

static int
style_parse_yaml1(struct style *st, struct lexer *lx)
{
	for (;;) {
		const struct style_option *so;
		struct token *key;
		int error = 0;

		if (lexer_if(lx, LEXER_EOF, NULL))
			break;
		if (lexer_peek_if(lx, Sequence, NULL))
			break;

		if (lexer_if(lx, DocumentBegin, NULL))
			continue;
		if (lexer_if(lx, DocumentEnd, NULL))
			continue;

		if (!lexer_peek(lx, &key))
			break;
		so = (struct style_option *)key->tk_token;
		if (so != NULL && so->so_scope != st->st_scope)
			break;
		if (so != NULL)
			error = so->so_parse(st, lx, so);
		if (error & (GOOD | SKIP)) {
			continue;
		} else if (error & FAIL) {
			break;
		} else {
			struct token *val;

			/* Best effort, try to continue parsing. */
			(void)lexer_pop(lx, &key);
			(void)lexer_if(lx, Colon, NULL);
			if (lexer_peek_if(lx, Sequence, NULL)) {
				/* Ignore sequences. */
				while (lexer_if(lx, Sequence, NULL) &&
				    lexer_pop(lx, &val))
					continue;
			} else {
				(void)lexer_pop(lx, &val);
			}
			lexer_error(lx, "unknown option %s",
			    lexer_serialize(lx, key));
		}
	}

	return 0;
}

static void
style_dump(const struct style *st)
{
	size_t noptions = sizeof(st->st_options) / sizeof(st->st_options[0]);
	size_t i;
	int scope = 0;

	for (i = 0; i < noptions; i++) {
		const struct style_option *so;
		const char *key;

		if (!st->st_options[i].isset)
			continue;

		key = style_keyword_str(i);
		so = yaml_find_keyword(key, strlen(key));

		if (so->so_scope != scope && so->so_scope != 0) {
			fprintf(stderr, "[s] %s:\n",
			    style_keyword_str(
			    (enum style_keyword)so->so_scope));
		}
		scope = so->so_scope;
		fprintf(stderr, "[s] ");
		if (scope != 0)
			fprintf(stderr, "  ");
		fprintf(stderr, "%s: ", key);

		switch (st->st_options[i].type) {
		case Integer:
			fprintf(stderr, "%u", st->st_options[i].val);
			break;
		default:
			fprintf(stderr, "%s",
			    style_keyword_str(st->st_options[i].val));
			break;
		}
		fprintf(stderr, "\n");
	}
}

static struct token *
yaml_read(struct lexer *lx, void *UNUSED(arg))
{
	struct lexer_state s;
	struct token *tk;
	unsigned char ch;

again:
	lexer_eat_lines_and_spaces(lx, NULL);

	tk = yaml_read_integer(lx);
	if (tk != NULL)
		return tk;

	s = lexer_get_state(lx);
	if (lexer_getc(lx, &ch))
		goto eof;

	if (ch == '#') {
		for (;;) {
			if (lexer_getc(lx, &ch))
				goto eof;
			if (ch == '\n')
				break;
		}
		goto again;
	}

	if (isalpha(ch) || ch == '_') {
		do {
			if (lexer_getc(lx, &ch))
				break;
		} while (!isspace(ch) && ch != ':');
		lexer_ungetc(lx);
		return yaml_keyword(lx, &s);
	}

	if (ch == '-' || ch == '.' || ch == ':') {
		unsigned char needle = ch;

		do {
			if (lexer_getc(lx, &ch))
				goto eof;
		} while (ch == needle);
		lexer_ungetc(lx);
		return yaml_keyword(lx, &s);
	}

	if (ch == '\'') {
		s = lexer_get_state(lx); /* discard '\'' */
		for (;;) {
			if (lexer_getc(lx, &ch))
				goto eof;
			if (ch == '\'')
				break;
		}
		lexer_ungetc(lx);
		tk = yaml_keyword(lx, &s);
		if (tk->tk_type == Unknown) {
			token_rele(tk);
			tk = NULL;
		}
		if (tk == NULL) {
			tk = lexer_emit(lx, &s,
			    &(struct token){.tk_type = String});
		}
		lexer_getc(lx, &ch); /* discard '\'' */
		return tk;
	}

	tk = lexer_emit(lx, &s, &(struct token){
	    .tk_type	= Unknown,
	});
	lexer_error(lx, "unknown token %s", lexer_serialize(lx, tk));
	token_rele(tk);
	return NULL;

eof:
	tk = lexer_emit(lx, &s, NULL);
	tk->tk_type = LEXER_EOF;
	return tk;
}

static struct token *
yaml_read_integer(struct lexer *lx)
{
	struct lexer_state s;
	struct token *tk;
	int digit = 0;
	int overflow = 0;
	int peek = 0;
	int sign = 1;
	int string = 0;
	unsigned char ch;

	s = lexer_get_state(lx);

	if (lexer_getc(lx, &ch))
		return NULL;
	if (ch == '\'') {
		if (lexer_getc(lx, &ch))
			return NULL;
		string = 1;
	}
	if (isdigit(ch)) {
		peek = 1;
	} else if (ch == '-' && lexer_getc(lx, &ch) == 0) {
		sign = -1;
		peek = isdigit(ch);
		if (!peek)
			lexer_ungetc(lx);
	}
	if (!peek) {
		lexer_ungetc(lx);
		if (string)
			lexer_ungetc(lx);
		return NULL;
	}

	while (isdigit(ch)) {
		int x = ch - '0';

		if (i32_mul_overflow(digit, 10, &digit) ||
		    i32_add_overflow(digit, x, &digit))
			overflow = 1;

		if (lexer_getc(lx, &ch))
			return NULL;
	}
	if (!string)
		lexer_ungetc(lx);

	if (i32_mul_overflow(digit, sign, &digit))
		overflow = 1;

	tk = lexer_emit(lx, &s, NULL);
	tk->tk_type = Integer;
	tk->tk_int = digit;
	if (overflow) {
		lexer_error(lx, "integer %s too large",
		    lexer_serialize(lx, tk));
	}
	return tk;
}

static char *
yaml_serialize(const struct token *tk)
{
	struct buffer *bf;
	char *buf;

	bf = buffer_alloc(128);
	if (bf == NULL)
		err(1, NULL);
	if (tk->tk_type < Last) {
		buffer_printf(bf, "Keyword");
	} else {
		buffer_printf(bf, "%s",
		    yaml_type_str((enum yaml_type)tk->tk_type));
	}
	buffer_printf(bf, "<%u:%u>(\"", tk->tk_lno, tk->tk_cno);
	strnice_buffer(bf, tk->tk_str, tk->tk_len);
	buffer_printf(bf, "\")");
	buf = buffer_str(bf);
	buffer_free(bf);
	return buf;
}

static struct token *
yaml_keyword(struct lexer *lx, const struct lexer_state *st)
{
	const struct style_option *so;
	struct token *tk;

	tk = lexer_emit(lx, st, NULL);
	so = yaml_find_keyword(tk->tk_str, tk->tk_len);
	if (so == NULL) {
		tk->tk_type = Unknown;
		return tk;
	}
	tk->tk_type = so->so_type;
	if (so->so_parse != NULL)
		tk->tk_token = (void *)so;
	return tk;
}

static const struct style_option *
yaml_find_keyword(const char *str, size_t len)
{
	size_t i;
	unsigned char slot;

	slot = (unsigned char)str[0];
	if (keywords[slot] == NULL)
		return NULL;
	for (i = 0; i < VECTOR_LENGTH(keywords[slot]); i++) {
		struct style_option *so = &keywords[slot][i];

		if (len == so->so_len && strncmp(so->so_key, str, len) == 0)
			return so;
	}
	return NULL;
}

static int
parse_bool(struct style *st, struct lexer *lx, const struct style_option *so)
{
	struct token *key, *val;

	if (!lexer_if(lx, so->so_type, &key))
		return NONE;
	if (!lexer_expect(lx, Colon, NULL))
		return FAIL;
	if (!lexer_if(lx, True, &val) && !lexer_if(lx, False, &val)) {
		(void)lexer_pop(lx, &val);
		lexer_error(lx, "unknown value %s for option %s",
		    lexer_serialize(lx, val), lexer_serialize(lx, key));
		return SKIP;
	}
	style_set(st, key->tk_type, None, (unsigned int)val->tk_type);
	return GOOD;
}

static int
parse_enum(struct style *st, struct lexer *lx, const struct style_option *so)
{
	struct token *key, *val;
	const int *v;

	if (!lexer_if(lx, so->so_type, &key))
		return NONE;
	if (!lexer_expect(lx, Colon, NULL))
		return FAIL;

	for (v = so->so_val; *v != 0; v++) {
		if (lexer_if(lx, *v, &val)) {
			style_set(st, key->tk_type, None,
			    (unsigned int)val->tk_type);
			return GOOD;
		}
	}

	(void)lexer_pop(lx, &val);
	lexer_error(lx, "unknown value %s for option %s",
	    lexer_serialize(lx, val), lexer_serialize(lx, key));
	return SKIP;
}

static int
parse_integer(struct style *st, struct lexer *lx, const struct style_option *so)
{
	struct token *key, *val;

	if (!lexer_if(lx, so->so_type, &key))
		return NONE;
	if (!lexer_expect(lx, Colon, NULL))
		return FAIL;
	if (!lexer_expect(lx, Integer, &val)) {
		(void)lexer_pop(lx, &val);
		lexer_error(lx, "unknown value %s for option %s",
		    lexer_serialize(lx, val), lexer_serialize(lx, key));
		return SKIP;
	}
	style_set(st, key->tk_type, Integer, (unsigned int)val->tk_int);
	return GOOD;
}

static int
parse_string(struct style *UNUSED(st), struct lexer *lx,
    const struct style_option *so)
{
	struct token *key, *val;

	if (!lexer_if(lx, so->so_type, &key))
		return NONE;
	if (!lexer_expect(lx, Colon, NULL))
		return FAIL;
	if (!lexer_expect(lx, String, &val)) {
		(void)lexer_pop(lx, &val);
		lexer_error(lx, "unknown value %s for option %s",
		    lexer_serialize(lx, val), lexer_serialize(lx, key));
		return SKIP;
	}
	return GOOD;
}

static int
parse_nested(struct style *st, struct lexer *lx, const struct style_option *so)
{
	int scope;

	if (!lexer_if(lx, so->so_type, NULL))
		return NONE;
	if (!lexer_expect(lx, Colon, NULL))
		return FAIL;
	scope = st->st_scope;
	st->st_scope = so->so_type;
	style_parse_yaml1(st, lx);
	st->st_scope = scope;
	return GOOD;
}

static int
parse_AlignOperands(struct style *st, struct lexer *lx,
    const struct style_option *so)
{
	int error;

	error = parse_enum(st, lx, so);
	if (error & GOOD) {
		if (st->st_options[AlignOperands].val == True) {
			style_set(st, AlignOperands, None, (unsigned int)Align);
		} else if (st->st_options[AlignOperands].val == False) {
			style_set(st, AlignOperands, None,
			    (unsigned int)DontAlign);
		}
	}
	return error;
}

static int
parse_ColumnLimit(struct style *st, struct lexer *lx,
    const struct style_option *so)
{
	int error;

	error = parse_integer(st, lx, so);
	if ((error & GOOD) && st->st_options[ColumnLimit].val == 0)
		style_set(st, ColumnLimit, Integer, UINT_MAX);
	return error;
}

static int
parse_IncludeCategories(struct style *st, struct lexer *lx,
    const struct style_option *so)
{
	int scope;

	if (!lexer_if(lx, so->so_type, NULL))
		return NONE;
	if (!lexer_expect(lx, Colon, NULL))
		return FAIL;

	scope = st->st_scope;
	st->st_scope = so->so_type;
	while (lexer_if(lx, Sequence, NULL))
		style_parse_yaml1(st, lx);
	st->st_scope = scope;
	return GOOD;
}

static const char *
yaml_type_str(enum yaml_type type)
{
	switch (type) {
#define OP(type, ...) case type: return #type;
	FOR_YAML_TYPES(OP)
#undef OP
	}
	if (type == LEXER_EOF)
		return "EOF";
	return NULL;
}

const char *
style_keyword_str(enum style_keyword keyword)
{
	switch (keyword) {
#define DO(style) case style: return #style;
	FOR_STYLES(DO)
#undef DO
	default:
		break;
	}
	return "Unknown";
}
