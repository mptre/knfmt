#include "style.h"

#include "config.h"

#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "libks/arena-buffer.h"
#include "libks/arena.h"
#include "libks/arithmetic.h"
#include "libks/buffer.h"
#include "libks/compiler.h"
#include "libks/vector.h"

#include "fs.h"
#include "lexer.h"
#include "options.h"
#include "token.h"
#include "trace.h"
#include "util.h"

#define style_trace(st, fmt, ...) \
	trace_no_func('s', (st)->op, (fmt), __VA_ARGS__)

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
	struct arena_scope		*eternal_scope;
	struct arena			*scratch;
	const struct options		*op;
	int				 scope;
	int				 depth;
	struct {
		int		type;
		unsigned int	isset;
		unsigned int	val;
	} options[Last];
	VECTOR(struct include_category)	 include_categories;
};

struct include_category {
	const char		*pattern;
	regex_t			 regex;
	struct include_priority	 priority;
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

struct yaml_token {
	const struct style_option	*so;
	union {
		int32_t		i32;
		uint32_t	u32;
	} integer;
};

static void	style_free(void *);
static void	style_defaults(struct style *);
static void	style_set(struct style *, int, int, unsigned int);
static int	style_parse_yaml(struct style *, const char *,
    const struct buffer *);
static int	style_parse_yaml_documents(struct style *, struct lexer *, int);
static void	style_dump(const struct style *);
static void	style_dump_IncludeCategories(const struct style *);

static struct token			*yaml_read(struct lexer *, void *);
static struct token			*yaml_read_integer(struct lexer *);
static struct token			*yaml_token_alloc(struct arena_scope *,
    const struct token *);
static const char			*yaml_token_serialize(
    struct arena_scope *, const struct token *);
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
static int	parse_integer_impl(struct style *, struct lexer *,
    const struct style_option *, struct token **, struct token **);
static int	parse_string(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_nested(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_AlignOperands(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_BasedOnStyle(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_ColumnLimit(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_IncludeCategories(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_IncludeGuards(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_Priority(struct style *, struct lexer *,
    const struct style_option *);
static int	parse_Regex(struct style *, struct lexer *,
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

		{ S(BasedOnStyle), parse_BasedOnStyle,
		  { LLVM, Google, Chromium, Mozilla, WebKit, Microsoft, GNU,
		    InheritParentConfig, OpenBSD } },

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

		{ S(IncludeBlocks), parse_enum,
		  { Merge, Preserve, Regroup } },

		{ S(IncludeCategories), parse_IncludeCategories, {0} },
		{ N(IncludeCategories, CaseSensitive), parse_bool, {0} },
		{ N(IncludeCategories, Priority), parse_Priority, {0} },
		{ N(IncludeCategories, Regex), parse_Regex, {0} },
		{ N(IncludeCategories, SortPriority), parse_Priority, {0} },

		{ S(IncludeGuards), parse_IncludeGuards, {0} },

		{ S(IndentWidth), parse_integer, {0} },

		{ S(Language), parse_enum,
		  { Cpp, CSharp, Java, JavaScript, Json, ObjC, Proto, TableGen,
		    TextProto, Verilog} },

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
		{ E(CSharp) },
		{ E(Chromium) },
		{ E(Cpp) },
		{ E(Custom) },
		{ E(DontAlign) },
		{ E(ForContinuationAndIndentation) },
		{ E(ForIndentation) },
		{ E(GNU) },
		{ E(Google) },
		{ E(InheritParentConfig) },
		{ E(Java) },
		{ E(JavaScript) },
		{ E(Json) },
		{ E(LLVM) },
		{ E(Left) },
		{ E(Linux) },
		{ E(Merge) },
		{ E(Microsoft) },
		{ E(Mozilla) },
		{ E(MultiLine) },
		{ E(Never) },
		{ E(NonAssignment) },
		{ E(None) },
		{ E(ObjC) },
		{ E(OpenBSD) },
		{ E(Preserve) },
		{ E(Proto) },
		{ E(Regroup) },
		{ E(Right) },
		{ E(Stroustrup) },
		{ E(TableGen) },
		{ E(TextProto) },
		{ E(TopLevel) },
		{ E(TopLevelDefinitions) },
		{ E(Verilog) },
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

void
style_dump_keywords(struct buffer *bf)
{
	size_t nslots = sizeof(keywords) / sizeof(keywords[0]);
	size_t i;

	for (i = 0; i < nslots; i++) {
		VECTOR(struct style_option) options = keywords[i];
		size_t j;

		if (options == NULL)
			continue;
		for (j = 0; j < VECTOR_LENGTH(options); j++) {
			const struct style_option *so = &options[j];

			buffer_printf(bf, "\"%.*s\"\n",
			    (int)so->so_len, so->so_key);
		}
	}
}

struct style *
style_parse(const char *path, struct arena_scope *eternal_scope,
    struct arena *scratch, const struct options *op)
{
	struct buffer *bf = NULL;
	struct style *st;

	arena_scope(scratch, s);

	if (path != NULL) {
		bf = arena_buffer_read(&s, path);
	} else {
		int fd;

		path = ".clang-format";
		fd = searchpath(path, NULL);
		if (fd != -1) {
			bf = arena_buffer_read_fd(&s, fd);
			close(fd);
		}
	}
	st = style_parse_buffer(bf, path, eternal_scope, scratch, op);
	if (st != NULL && options_trace_level(op, 's') >= 2)
		style_dump(st);
	return st;
}

struct style *
style_parse_buffer(const struct buffer *bf, const char *path,
    struct arena_scope *eternal_scope, struct arena *scratch,
    const struct options *op)
{
	struct style *st;

	st = arena_calloc(eternal_scope, 1, sizeof(*st));
	arena_cleanup(eternal_scope, style_free, st);
	st->eternal_scope = eternal_scope;
	st->scratch = scratch;
	st->op = op;
	if (VECTOR_INIT(st->include_categories))
		err(1, NULL);
	style_defaults(st);
	if (bf == NULL) {
		 /*
		  * Only apply default style if no clang-format configuration
		  * file is present.
		  */
		style_set(st, BasedOnStyle, None, OpenBSD);
	} else {
		/* Errors are not considered fatal. */
		(void)style_parse_yaml(st, path, bf);
	}
	return st;
}

static void
style_free(void *arg)
{
	struct style *st = arg;

	while (!VECTOR_EMPTY(st->include_categories)) {
		struct include_category *ic;

		ic = VECTOR_POP(st->include_categories);
		regfree(&ic->regex);
	}
	VECTOR_FREE(st->include_categories);
}

unsigned int
style(const struct style *st, int option)
{
	assert(option < Last);
	return st->options[option].val;
}

int
style_brace_wrapping(const struct style *st, int option)
{
	switch (st->options[BreakBeforeBraces].val) {
	case Linux:
		switch (option) {
		case AfterFunction:
			return 1;
		}
		break;
	}
	return st->options[option].val == True;
}

static int
priority_cmp(const int *aa, const int *bb)
{
	int a = *aa;
	int b = *bb;

	if (a < b)
		return -1;
	if (a > b)
		return 1;
	return 0;
}

int *
style_include_priorities(const struct style *st)
{
	VECTOR(int) priorities;
	int *dst;
	size_t i, n;

	if (VECTOR_INIT(priorities))
		err(1, NULL);

	/* Add default min and max priorities. */
	dst = VECTOR_ALLOC(priorities);
	if (dst == NULL)
		err(1, NULL);
	*dst = 0;
	dst = VECTOR_ALLOC(priorities);
	if (dst == NULL)
		err(1, NULL);
	*dst = INT_MAX;

	n = VECTOR_LENGTH(st->include_categories);
	for (i = 0; i < n; i++) {
		const struct include_category *ic = &st->include_categories[i];

		dst = VECTOR_ALLOC(priorities);
		if (dst == NULL)
			err(1, NULL);
		*dst = ic->priority.group;
	}
	VECTOR_SORT(priorities, priority_cmp);
	return priorities;
}

static int
is_main_include(const struct style *st, const char *include_path,
    const char *path)
{
	const char *basename, *dot, *filename, *include_main,
		   *path_without_extension, *slash;

	arena_scope(st->scratch, s);

	/* Transform path "a/b.c" into "a/b.h". */
	dot = strrchr(path, '.');
	if (dot == NULL)
		return 0;
	path_without_extension = arena_strndup(&s, path, (size_t)(dot - path));
	include_main = arena_sprintf(&s, "\"%s.h\"", path_without_extension);
	if (strcmp(include_path, include_main) == 0)
		return 1;

	/* Transform path "a/b.c" into "b.h". */
	slash = strrchr(path, '/');
	filename = slash != NULL ? arena_strdup(&s, &slash[1]) : path;
	dot = strrchr(filename, '.');
	if (dot == NULL)
		return 0;
	basename = arena_strndup(&s, filename, (size_t)(dot - filename));
	include_main = arena_sprintf(&s, "\"%s.h\"", basename);
	if (strcmp(include_path, include_main) == 0)
		return 1;

	return 0;
}

struct include_priority
style_include_priority(const struct style *st, const char *include_path,
    const char *path)
{
	size_t i, n;

	if (is_main_include(st, include_path, path))
		return (struct include_priority){.group = 0, .sort = 0};

	n = VECTOR_LENGTH(st->include_categories);
	for (i = 0; i < n; i++) {
		const struct include_category *ic = &st->include_categories[i];
		int error;

		error = regexec(&ic->regex, include_path, 0, NULL, 0);
		if (error == 0)
			return ic->priority;
	}

	return (struct include_priority){.group = INT_MAX, .sort = INT_MAX};
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
		{ IncludeBlocks,		Preserve },
		{ IndentWidth,			8 },
		{ SortIncludes,			Never },
		{ UseTab,			Always },
	};
	size_t ndefaults = sizeof(defaults) / sizeof(defaults[0]);
	size_t i;

	for (i = 0; i < ndefaults; i++)
		st->options[defaults[i].key].val = defaults[i].val;
}

static void
style_set(struct style *st, int option, int type, unsigned int val)
{
	st->options[option].type = type;
	st->options[option].isset = 1;
	st->options[option].val = val;
}

static int
style_parse_yaml(struct style *st, const char *path, const struct buffer *bf)
{
	struct lexer *lx;
	int error = 0;

	lx = lexer_alloc(&(const struct lexer_arg){
	    .path		= path,
	    .bf			= bf,
	    .eternal_scope	= st->eternal_scope,
	    .op			= st->op,
	    .error_flush	= options_trace_level(st->op, 's') > 0,
	    .callbacks		= {
		.read		= yaml_read,
		.alloc		= yaml_token_alloc,
		.serialize	= yaml_token_serialize,
		.arg		= st,
	    },
	});
	if (lx == NULL) {
		error = 1;
		goto out;
	}
	error = style_parse_yaml_documents(st, lx, 0);

out:
	return error;
}

static enum style_keyword
yaml_document_language(struct lexer *lx)
{
	struct lexer_state s;
	struct token *tk;
	enum style_keyword language = None;

	lexer_peek_enter(lx, &s);
	do {
		if (lexer_if(lx, LEXER_EOF, NULL))
			break;
		if (lexer_if(lx, DocumentBegin, NULL))
			break;

		if (lexer_if(lx, Language, NULL) &&
		    lexer_if(lx, Colon, NULL) &&
		    lexer_pop(lx, &tk)) {
			language = (enum style_keyword)tk->tk_type;
			break;
		}
	} while (lexer_pop(lx, &tk));
	lexer_peek_leave(lx, &s);

	return language;
}

static void
skip_document(struct lexer *lx)
{
	struct token *tk;

	do {
		if (lexer_if(lx, LEXER_EOF, NULL) ||
		    lexer_if(lx, DocumentBegin, NULL))
			break;
	} while (lexer_pop(lx, &tk));
}

static int
style_parse_yaml_documents(struct style *st, struct lexer *lx, int nested)
{
	int error;

	for (;;) {
		const struct style_option *so;
		struct token *key;

		error = 0;

		if (lexer_if(lx, LEXER_EOF, NULL))
			break;

		if (!nested) {
			enum style_keyword language;

			/*
			 * Only honor documents applicable to all languages or
			 * C++ which is applicable to C as well according to
			 * clang-format.
			 */
			language = yaml_document_language(lx);
			if (language != None && language != Cpp) {
				skip_document(lx);
				continue;
			}
		}

		if (lexer_peek_if(lx, Sequence, NULL))
			break;

		if (lexer_if(lx, DocumentBegin, NULL))
			continue;
		if (lexer_if(lx, DocumentEnd, NULL))
			continue;

		if (!lexer_peek(lx, &key)) {
			error = FAIL;
			break;
		}
		so = token_data(key, struct yaml_token)->so;
		if (so != NULL && so->so_scope != st->scope)
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
			lexer_error(lx, key, __func__, __LINE__,
			    "unknown option %s", lexer_serialize(lx, key));
		}
	}

	return error;
}

static void
style_dump(const struct style *st)
{
	size_t noptions = sizeof(st->options) / sizeof(st->options[0]);
	size_t i;
	int scope = 0;

	for (i = 0; i < noptions; i++) {
		const struct style_option *so;
		const char *key;

		if (!st->options[i].isset)
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

		if (so->so_type == IncludeCategories) {
			style_dump_IncludeCategories(st);
		} else {
			switch (st->options[i].type) {
			case Integer:
				fprintf(stderr, "%u", st->options[i].val);
				break;
			default:
				fprintf(stderr, "%s",
				    style_keyword_str(st->options[i].val));
				break;
			}
			fprintf(stderr, "\n");
		}
	}
}

static void
style_dump_IncludeCategories(const struct style *st)
{
	size_t i;

	fprintf(stderr, "\n");

	for (i = 0; i < VECTOR_LENGTH(st->include_categories); i++) {
		const struct include_category *ic = &st->include_categories[i];

		fprintf(stderr, "[s] - Regex: '%s'\n", ic->pattern);
		fprintf(stderr, "[s]   Priority: %d\n", ic->priority.group);
		fprintf(stderr, "[s]   SortPriority: %d\n", ic->priority.sort);
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
	lexer_error(lx, tk, __func__, __LINE__,
	    "unknown token %s", lexer_serialize(lx, tk));
	token_rele(tk);
	return NULL;

eof:
	return lexer_emit(lx, &s, &(struct token){.tk_type = LEXER_EOF});
}

static struct token *
yaml_read_integer(struct lexer *lx)
{
	struct lexer_state s;
	struct token *tk;
	int32_t integer = 0;
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

		if (KS_i32_mul_overflow(integer, 10, &integer) ||
		    KS_i32_add_overflow(integer, x, &integer))
			overflow = 1;

		if (lexer_getc(lx, &ch))
			return NULL;
	}
	if (!string)
		lexer_ungetc(lx);

	if (KS_i32_mul_overflow(integer, sign, &integer))
		overflow = 1;

	tk = lexer_emit(lx, &s, &(struct token){.tk_type = Integer});
	token_data(tk, struct yaml_token)->integer.i32 = integer;
	if (overflow) {
		lexer_error(lx, tk, __func__, __LINE__,
		    "integer %s too large", lexer_serialize(lx, tk));
	}
	return tk;
}

static struct token *
yaml_token_alloc(struct arena_scope *s, const struct token *def)
{
	return token_alloc(s, sizeof(struct yaml_token), def);
}

static const char *
yaml_token_serialize(struct arena_scope *s, const struct token *tk)
{
	struct buffer *bf;

	bf = arena_buffer_alloc(s, 128);
	if (tk->tk_type < Last) {
		buffer_printf(bf, "Keyword");
	} else {
		buffer_printf(bf, "%s",
		    yaml_type_str((enum yaml_type)tk->tk_type));
	}
	if (tk->tk_str != NULL) {
		buffer_printf(bf, "<%u:%u>(\"", tk->tk_lno, tk->tk_cno);
		strnice_buffer(bf, tk->tk_str, tk->tk_len);
		buffer_printf(bf, "\")");
	}
	return buffer_str(bf);
}

static struct token *
yaml_keyword(struct lexer *lx, const struct lexer_state *st)
{
	const struct style_option *so;
	struct token *tk;
	const char *buf;
	size_t buflen;

	buf = lexer_buffer_slice(lx, st, &buflen);
	if (buf == NULL)
		goto unknown;
	so = yaml_find_keyword(buf, buflen);
	if (so == NULL)
		goto unknown;

	tk = lexer_emit(lx, st, &(struct token){.tk_type = so->so_type});
	if (so->so_parse != NULL)
		token_data(tk, struct yaml_token)->so = so;
	return tk;

unknown:
	return lexer_emit(lx, st, &(struct token){.tk_type = Unknown});
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
		lexer_error(lx, val, __func__, __LINE__,
		    "unknown value %s for option %s",
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
	lexer_error(lx, val, __func__, __LINE__,
	    "unknown value %s for option %s",
	    lexer_serialize(lx, val), lexer_serialize(lx, key));
	return SKIP;
}

static int
parse_integer(struct style *st, struct lexer *lx, const struct style_option *so)
{
	struct token *key, *val;
	int error;

	error = parse_integer_impl(st, lx, so, &key, &val);
	if ((error & GOOD) == 0)
		return error;
	style_set(st, key->tk_type, Integer,
	    token_data(val, struct yaml_token)->integer.u32);
	return GOOD;
}

static int
parse_integer_impl(struct style *UNUSED(st), struct lexer *lx,
    const struct style_option *so, struct token **key, struct token **val)
{
	if (!lexer_if(lx, so->so_type, key))
		return NONE;
	if (!lexer_expect(lx, Colon, NULL))
		return FAIL;
	if (!lexer_expect(lx, Integer, val)) {
		(void)lexer_pop(lx, val);
		lexer_error(lx, *val, __func__, __LINE__,
		    "unknown value %s for option %s",
		    lexer_serialize(lx, *val), lexer_serialize(lx, *key));
		return SKIP;
	}
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
		lexer_error(lx, val, __func__, __LINE__,
		    "unknown value %s for option %s",
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
	scope = st->scope;
	st->scope = so->so_type;
	style_parse_yaml_documents(st, lx, 1);
	st->scope = scope;
	return GOOD;
}

static int
parse_AlignOperands(struct style *st, struct lexer *lx,
    const struct style_option *so)
{
	int error;

	error = parse_enum(st, lx, so);
	if (error & GOOD) {
		if (st->options[AlignOperands].val == True) {
			style_set(st, AlignOperands, None, (unsigned int)Align);
		} else if (st->options[AlignOperands].val == False) {
			style_set(st, AlignOperands, None,
			    (unsigned int)DontAlign);
		}
	}
	return error;
}

static struct buffer *
clang_format_dump_style(struct style *st, enum style_keyword based_on_style,
    struct arena_scope *s)
{
	struct buffer *bf;
	pid_t pid;
	int pip[2];
	int status;

	/* NOLINTNEXTLINE(android-cloexec-pipe) */
	if (pipe(pip) == -1)
		err(1, "pipe");
	pid = fork();
	if (pid == -1)
		err(1, "fork");
	if (pid == 0) {
		const char *clang_format_style;

		close(0);
		close(pip[0]);
		if (dup2(pip[1], 1) == -1)
			err(1, "dup2");
		if (dup2(pip[1], 2) == -1)
			err(1, "dup2");

		clang_format_style = arena_sprintf(s, "-style=%s",
		    style_keyword_str(based_on_style));
		execlp("clang-format", "clang-format", "-dump-config",
		    clang_format_style, NULL);
		_exit(1);
	}

	close(pip[1]);
	bf = arena_buffer_read_fd(s, pip[0]);
	close(pip[0]);
	if (waitpid(pid, &status, 0) == -1)
		err(1, "waitpid");
	if (bf != NULL && (!WIFEXITED(status) || WEXITSTATUS(status) != 0)) {
		struct buffer_getline it = {0};
		const char *line;

		arena_scope(st->scratch, scratch_scope);

		while ((line = arena_buffer_getline(&scratch_scope, bf, &it)) !=
		    NULL)
			style_trace(st, "clang-format: %s", line);
		return NULL;
	}
	return bf;
}

static int
parse_BasedOnStyle(struct style *st, struct lexer *lx,
    const struct style_option *so)
{
	struct buffer *bf;
	enum style_keyword based_on_style;
	int error;

	arena_scope(st->scratch, s);

	error = parse_enum(st, lx, so);
	if ((error & GOOD) == 0)
		return error;

	if (st->depth > 0) {
		struct token *ctx = NULL;

		(void)lexer_back(lx, &ctx);
		lexer_error(lx, ctx, __func__, __LINE__,
		    "ignoring nested style");
		return SKIP;
	}

	based_on_style = style(st, BasedOnStyle);
	if (based_on_style == OpenBSD || based_on_style == InheritParentConfig)
		return GOOD;

	style_trace(st, "based on %s style", style_keyword_str(based_on_style));
	bf = clang_format_dump_style(st, based_on_style, &s);
	if (bf == NULL)
		return SKIP;
	st->depth++;
	error = style_parse_yaml(st, style_keyword_str(based_on_style), bf);
	st->depth--;
	return error ? FAIL : GOOD;
}

static int
parse_ColumnLimit(struct style *st, struct lexer *lx,
    const struct style_option *so)
{
	int error;

	error = parse_integer(st, lx, so);
	if ((error & GOOD) && st->options[ColumnLimit].val == 0)
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

	scope = st->scope;
	st->scope = so->so_type;
	while (lexer_if(lx, Sequence, NULL)) {
		if (VECTOR_CALLOC(st->include_categories) == NULL)
			err(1, NULL);
		style_parse_yaml_documents(st, lx, 1);
	}
	st->scope = scope;
	/* Only used by style_dump(). */
	style_set(st, IncludeCategories, None, None);
	return GOOD;
}

static int
parse_IncludeGuards(struct style *st, struct lexer *lx,
    const struct style_option *so)
{
	struct token *val;
	int error;

	error = parse_integer(st, lx, so);
	if ((error & GOOD) == 0)
		return error;
	if (lexer_back(lx, &val) &&
	    token_data(val, struct yaml_token)->integer.i32 <= 0) {
		style_set(st, IncludeGuards, Integer, 0);
		lexer_error(lx, val, __func__, __LINE__,
		    "integer %s too small", lexer_serialize(lx, val));
		return FAIL;
	}
	return GOOD;
}

static int
parse_Priority(struct style *st, struct lexer *lx,
    const struct style_option *so)
{
	struct include_category *ic;
	struct token *key, *val;
	int error, priority;

	assert(so->so_type == Priority || so->so_type == SortPriority);

	error = parse_integer_impl(st, lx, so, &key, &val);
	if ((error & GOOD) == 0)
		return error;

	ic = VECTOR_LAST(st->include_categories);
	if (ic == NULL)
		return FAIL; /* UNREACHABLE */
	priority = token_data(val, struct yaml_token)->integer.i32;
	if (so->so_type == Priority) {
		ic->priority.group = priority;
		ic->priority.sort = priority;
	} else if (so->so_type == SortPriority) {
		ic->priority.sort = priority;
	}
	return GOOD;
}

static int
parse_Regex(struct style *st, struct lexer *lx, const struct style_option *so)
{
	struct include_category *ic;
	struct token *tk;
	int error;

	error = parse_string(st, lx, so);
	if ((error & GOOD) == 0)
		return error;
	if (!lexer_back(lx, &tk))
		return FAIL;

	ic = VECTOR_LAST(st->include_categories);
	ic->pattern = arena_strndup(st->eternal_scope, tk->tk_str, tk->tk_len);
	/* Allow the pattern to be redefined. */
	regfree(&ic->regex);
	error = regcomp(&ic->regex, ic->pattern,
	    REG_EXTENDED | REG_NOSUB | REG_ICASE);
	if (error) {
		char errbuf[128] = {0};

		if (error == REG_EPAREN) {
			/* Use platform agnostic error for testing. */
			(void)snprintf(errbuf, sizeof(errbuf),
			    "parentheses not balanced");
		} else {
			regerror(error, &ic->regex, errbuf,
			    sizeof(errbuf));
		}
		lexer_error(lx, tk, __func__, __LINE__, "%s", errbuf);
		return FAIL;
	}
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
