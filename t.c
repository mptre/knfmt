#include "config.h"

#include <string.h>

#include "libks/arena-buffer.h"
#include "libks/arena.h"
#include "libks/buffer.h"
#include "libks/expect.h"
#include "libks/list.h"

#include "arenas.h"
#include "clang.h"
#include "doc.h"
#include "expr.h"
#include "lexer.h"
#include "options.h"
#include "parser-attributes.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "parser-type.h"
#include "parser.h"
#include "simple.h"
#include "style.h"
#include "token.h"
#include "util.h"

struct context {
	struct options		 op;
	struct buffer		*bf;
	struct style		*st;
	struct simple		*si;
	struct clang		*cl;
	struct lexer		*lx;
	struct parser		*pr;
	struct arenas		 arena;
};

#define test_parser_expr(a, b) \
	test_parser_expr_impl(&ctx, (a), (b), __LINE__)
static void	test_parser_expr_impl(struct context *, const char *,
    const char *, int);

#define test_parser_type(a, b) \
	test_parser_type_impl(&ctx, (a), (b), 0, 1, __LINE__)
#define test_parser_type_flags(a, b, c) \
	test_parser_type_impl(&ctx, (b), (c), (a), 1, __LINE__)
#define test_parser_type_error(a) \
	test_parser_type_impl(&ctx, (a), "", 0, 0, __LINE__)
static void	test_parser_type_impl(struct context *, const char *,
    const char *, unsigned int, int, int);

#define test_parser_attributes_peek(a, b) \
	test_parser_attributes_peek_impl(&ctx, (a), (b), 1, 0, __LINE__)
#define test_parser_attributes_peek_flags(a, b, c) \
	test_parser_attributes_peek_impl(&ctx, (b), (c), 1, (a), __LINE__)
#define test_parser_attributes_peek_flags_error(a, b) \
	test_parser_attributes_peek_impl(&ctx, (b), "", 0, (a), __LINE__)
static void	test_parser_attributes_peek_impl(struct context *, const char *,
    const char *, int, unsigned int, int);

#define test_lexer_read(a, b) \
	test_lexer_read_impl(&ctx, (a), (b), __LINE__)
static void	test_lexer_read_impl(struct context *, const char *,
    const char *, int);

struct test_token_move {
	const char	*src;
	int		 target;
	int		 move;
	const char	*want[16];
};

#define test_token_position_after(a) \
	test_token_position_after_impl(&ctx, (a), __LINE__)
static void	test_token_position_after_impl(struct context *,
    struct test_token_move *, int);

#define test_style(a, b, c) \
	test_style_impl(&ctx, (a), (b), (c), __LINE__)
static void	test_style_impl(struct context *, const char *, int, int, int);

#define test_strwidth(a, b, c) \
	test_strwidth_impl((a), (b), (c), __LINE__)
static void	test_strwidth_impl(const char *, size_t, size_t, int);

#define test_path_slice(a, b, c) \
	test_path_slice_impl(&ctx, (a), (b), (c), __LINE__)
static void	test_path_slice_impl(struct context *, const char *,
    unsigned int, const char *, int);

#define test_is_path_header(a, b) \
	test_is_path_header_impl((a), (b), __LINE__)
static void	test_is_path_header_impl(const char *, int, int);

#define test_token_branch_unlink() \
	test_token_branch_impl(&ctx)
static void	test_token_branch_impl(struct context *);

static void	test_token_serialize(struct context *);
static void	test_clang_token_serialize(struct context *);

static void	context_alloc(struct context *);
static void	context_free(struct context *);
static void	context_init(struct context *, const char *,
    struct arena_scope *);

static void	assert_token_move(struct context *, const char **);
static int	find_token(struct lexer *, int, struct token **);

static const char	*tokens_concat(
    const char *(*)(struct arena_scope *, const struct token *),
    const struct token *, const struct token *, struct arena_scope *);
static const char	*token_serialize_literal(struct arena_scope *,
    const struct token *);
static const char	*token_serialize_no_flags(struct arena_scope *,
    const struct token *);
static const char	*token_serialize_position(struct arena_scope *,
    const struct token *);

int
main(void)
{
	struct context ctx = {0};

	clang_init();
	expr_init();
	style_init();
	context_alloc(&ctx);

	test_parser_expr("1", "(1)");
	test_parser_expr("x", "(x)");
	test_parser_expr("\"x\"", "(\"x\")");
	test_parser_expr("\"x\" \"y\"", "((\"x\") (\"y\"))");
	test_parser_expr("x(1, 2)", "((x)(((1), (2))))");
	test_parser_expr("x = 1", "((x) = (1))");
	test_parser_expr("x += 1", "((x) += (1))");
	test_parser_expr("x -= 1", "((x) -= (1))");
	test_parser_expr("x *= 1", "((x) *= (1))");
	test_parser_expr("x /= 1", "((x) /= (1))");
	test_parser_expr("x %= 1", "((x) %= (1))");
	test_parser_expr("x <<= 1", "((x) <<= (1))");
	test_parser_expr("x >>= 1", "((x) >>= (1))");
	test_parser_expr("x &= 1", "((x) &= (1))");
	test_parser_expr("x ^= 1", "((x) ^= (1))");
	test_parser_expr("x |= 1", "((x) |= (1))");
	test_parser_expr("x ? 1 : 0", "((x) ? (1) : (0))");
	test_parser_expr("a(x ? 1 : 0, 3)", "((a)((((x) ? (1) : (0)), (3))))");
	test_parser_expr("x ?: 0", "((x) ?: (0))");
	test_parser_expr("x || y", "((x) || (y))");
	test_parser_expr("x && y", "((x) && (y))");
	test_parser_expr("x | y", "((x) | (y))");
	test_parser_expr("x|y", "((x)|(y))");
	test_parser_expr("x ^ y", "((x) ^ (y))");
	test_parser_expr("x & y", "((x) & (y))");
	test_parser_expr("x == y", "((x) == (y))");
	test_parser_expr("x != y", "((x) != (y))");
	test_parser_expr("x < y", "((x) < (y))");
	test_parser_expr("x <= y", "((x) <= (y))");
	test_parser_expr("x > y", "((x) > (y))");
	test_parser_expr("x >= y", "((x) >= (y))");
	test_parser_expr("x << y", "((x) << (y))");
	test_parser_expr("x >> y", "((x) >> (y))");
	test_parser_expr("x + y", "((x) + (y))");
	test_parser_expr("x - y", "((x) - (y))");
	test_parser_expr("x * y", "((x) * (y))");
	test_parser_expr("x*y", "((x)*(y))");
	test_parser_expr("x / y", "((x) / (y))");
	test_parser_expr("x/y", "((x)/(y))");
	test_parser_expr("x % y", "((x) % (y))");
	test_parser_expr("!x", "(!(x))");
	test_parser_expr("~x", "(~(x))");
	test_parser_expr("++x", "(++(x))");
	test_parser_expr("x++", "((x)++)");
	test_parser_expr("--x", "(--(x))");
	test_parser_expr("x--", "((x)--)");
	test_parser_expr("-x", "(-(x))");
	test_parser_expr("+x", "(+(x))");
	test_parser_expr("(int)x", "(((int))(x))");
	test_parser_expr("(struct s)x", "(((struct s))(x))");
	test_parser_expr("(struct s*)x", "(((struct s *))(x))");
	test_parser_expr("(char const* char*)x", "(((char const *char *))(x))");
	test_parser_expr("(foo_t)x", "(((foo_t))(x))");
	test_parser_expr("(foo_t *)x", "(((foo_t *))(x))");
	test_parser_expr("(pc) & ~mask", "(((pc)) & (~(mask)))");
	test_parser_expr("(int)(x) +\ny", "((((int))((x))) +\n(y))");
	test_parser_expr("(a), (b)", "(((a)), ((b)))");
	test_parser_expr("x * y", "((x) * (y))");
	test_parser_expr("x & y", "((x) & (y))");
	test_parser_expr("sizeof x", "(sizeof (x))");
	test_parser_expr("sizeof x * y", "((sizeof (x)) * (y))");
	test_parser_expr("sizeof char", "(sizeof (char))");
	test_parser_expr("sizeof char *c", "((sizeof (char)) * (c))");
	test_parser_expr("sizeof struct s", "(sizeof (struct s))");
	test_parser_expr("sizeof(x)", "(sizeof((x)))");
	test_parser_expr("sizeof(*x)", "(sizeof((*(x))))");
	test_parser_expr("sizeof(x y)", "(sizeof(((x) (y))))");
	test_parser_expr("(x)", "((x))");
	test_parser_expr("x()", "((x)())");
	test_parser_expr("x(\"x\" X)", "((x)(((\"x\") (X))))");
	test_parser_expr("x[1 + 2]", "((x)[((1) + (2))])");
	test_parser_expr("x->y", "((x)->(y))");
	test_parser_expr("x.y", "((x).(y))");

	test_parser_type("void", "void");
	test_parser_type("void *", "void *");
	test_parser_type("void *p", "void *");
	test_parser_type("size_t", "size_t");
	test_parser_type("size_t s)", "size_t");
	test_parser_type("size_t *", "size_t *");
	test_parser_type("size_t *p", "size_t *");
	test_parser_type("void main(int)", "void");
	test_parser_type("void main(int);", "void");
	test_parser_type("static foo void", "static foo void");
	test_parser_type("static void foo", "static void foo");
	test_parser_type("void foo f(void)", "void foo");
	test_parser_type("char[]", "char [ ]");
	test_parser_type("struct wsmouse_param[]", "struct wsmouse_param [ ]");
	test_parser_type("void)", "void");
	test_parser_type("void,", "void");
	test_parser_type("struct {,", "struct");
	test_parser_type("struct s,", "struct s");
	test_parser_type("struct s {,", "struct s");
	test_parser_type("struct s *,", "struct s *");
	test_parser_type("struct s **,", "struct s * *");
	test_parser_type("struct s ***,", "struct s * * *");
	test_parser_type("union {,", "union");
	test_parser_type("union u,", "union u");
	test_parser_type("union u {,", "union u");
	test_parser_type("const* char*", "const * char *");
	test_parser_type("char x[]", "char");
	test_parser_type("va_list ap;", "va_list");
	test_parser_type("struct s *M(v)", "struct s *");
	test_parser_type(
	    "const struct filterops *const sysfilt_ops[]",
	    "const struct filterops * const");
	test_parser_type("long __guard_local __attribute__", "long");
	test_parser_type("unsigned int f:1", "unsigned int");
	test_parser_type(
	    "usbd_status (*v)(void)",
	    "usbd_status ( * v ) ( void )");
	test_parser_type("register char", "register char");
	test_parser_type("...", "...");
	test_parser_type("int (*f[])(void)", "int ( * f [ ] ) ( void )");
	test_parser_type(
	    "int (* volatile f)(void);",
	    "int ( * volatile f ) ( void )");
	test_parser_type(
	    "void (*const *f)(void);",
	    "void ( * const * f ) ( void )");
	test_parser_type("int (**f)(void);", "int ( * * f ) ( void )");
	test_parser_type("int (*f(void))(void)", "int");
	test_parser_type(
	    "void (*f[1])(void)",
	    "void ( * f [ 1 ] ) ( void )");
	test_parser_type("void (*)", "void ( * )");
	test_parser_type("char (*v)[1]", "char ( * v ) [ 1 ]");
	test_parser_type(
	    "P256_POINT (*table)[16]",
	    "P256_POINT ( * table ) [ 16 ]");
	test_parser_type("char (*)[TP_BSIZE]", "char ( * ) [ TP_BSIZE ]");
	test_parser_type(
	    "STACK_OF(X509_EXTENSION) x",
	    "STACK_OF ( X509_EXTENSION ) x");
	test_parser_type(
	    "STACK_OF(X509_EXTENSION) *",
	    "STACK_OF ( X509_EXTENSION ) *");
	test_parser_type(
	    "const STACK_OF(X509_EXTENSION)*",
	    "const STACK_OF ( X509_EXTENSION ) *");
	test_parser_type(
	    "int\nmain(foo)\n\tfoo_t *",
	    "int");
	test_parser_type("u_int:4", "u_int");
	test_parser_type("LIST_ENTRY(list, s);", "LIST_ENTRY ( list , s )");
	test_parser_type("union [)]", "union");

	test_parser_type_flags(PARSER_TYPE_CAST,
	    "const foo_t)", "const foo_t");
	test_parser_type_flags(PARSER_TYPE_ARG,
	    "const foo_t)", "const");
	test_parser_type_flags(PARSER_TYPE_ARG,
	    "size_t)", "size_t");
	test_parser_type_flags(PARSER_TYPE_ARG,
	    "foo_t[]", "foo_t [ ]");
	test_parser_type_flags(PARSER_TYPE_ARG,
	    "regmatch_t[10]", "regmatch_t [ 10 ]");
	test_parser_type_flags(PARSER_TYPE_EXPR,
	    "const foo_t)", "const foo_t");

	test_parser_type_error("_asm volatile (");
	test_parser_type_error("*");
	test_parser_type_error("[");

	test_parser_attributes_peek(
	    "__attribute__((one))",
	    "__attribute__ ( ( one ) )");
	test_parser_attributes_peek(
	    "__attribute__((one)) __attribute__((two))",
	    "__attribute__ ( ( one ) ) __attribute__ ( ( two ) )");
	test_parser_attributes_peek_flags(PARSER_ATTRIBUTES_FUNC,
	    "printflike(3, 4) main(void) {}",
	    "printflike ( 3 , 4 )");
	test_parser_attributes_peek_flags_error(PARSER_ATTRIBUTES_FUNC,
	    "main(void) __attribute__((one))");
	test_parser_attributes_peek_flags_error(PARSER_ATTRIBUTES_FUNC,
	    "main(void);");
	test_parser_attributes_peek_flags_error(PARSER_ATTRIBUTES_FUNC,
	    "main(argc) int argc;");

	test_lexer_read("<", "LESS");
	test_lexer_read("<x", "LESS IDENT");
	test_lexer_read("<=", "LESSEQUAL");
	test_lexer_read("<<", "LESSLESS");
	test_lexer_read("<<=", "LESSLESSEQUAL");

	test_lexer_read(".", "PERIOD");
	test_lexer_read("..", "PERIOD PERIOD");
	test_lexer_read("...", "ELLIPSIS");
	test_lexer_read(".x", "PERIOD IDENT");
	test_lexer_read(".3", "LITERAL");

	test_lexer_read("__asm", "ASSEMBLY");
	test_lexer_read("__asm__", "ASSEMBLY");
	test_lexer_read("__attribute", "ATTRIBUTE");
	test_lexer_read("attribute", "IDENT");
	test_lexer_read("__declspec", "ATTRIBUTE");

	test_lexer_read("asm_inline", "ASSEMBLY");
	test_lexer_read("asm_volatile_goto", "ASSEMBLY");

	test_token_position_after((&(struct test_token_move){
	    .src	= "\tint a;\n\tchar b;\n",
	    .target	= TOKEN_SEMI,
	    .move	= TOKEN_CHAR,
	    .want	= {
		"INT<1:9>(int)",
		"  SPACE<1:12>( )",
		"IDENT<1:13>(a)",
		"SEMI<1:14>(;)",
		"  SPACE<1:15>(\\n)",
		"CHAR<1:9>(char)",
		"  SPACE<1:13>( )",
		"IDENT<2:14>(b)",
		"SEMI<2:15>(;)",
		"  SPACE<2:16>(\\n)",
	    },
	}));

	test_style("UseTab: Never", UseTab, Never);
	test_style("UseTab: 'Never'", UseTab, Never);
	test_style("ColumnLimit: '100'", ColumnLimit, 100);
	test_style("ColumnLimit: '-100'", ColumnLimit, -100);
	test_style("ColumnLimit: '100'\nColumnLimit: 200", ColumnLimit, 200);
	test_style("UseTab: 'Never'\nColumnLimit: '100'", ColumnLimit, 100);

	test_strwidth("int", 0, 3);
	test_strwidth("int\tx", 0, 9);
	test_strwidth("int\tx", 3, 9);
	test_strwidth("int\nx", 0, 1);
	test_strwidth("int\n", 0, 0);

	test_path_slice("", 1, "");
	test_path_slice("", 2, "");
	test_path_slice("file", 1, "file");
	test_path_slice("file", 2, "file");
	test_path_slice("dir/file", 2, "dir/file");
	test_path_slice("/dir/file", 2, "dir/file");
	test_path_slice("dir/file", 3, "dir/file");

	test_is_path_header("test.h", 1);
	test_is_path_header("test.c", 0);
	test_is_path_header("test.", 0);

	test_token_branch_unlink();
	test_token_serialize(&ctx);
	test_clang_token_serialize(&ctx);

	context_free(&ctx);
	style_shutdown();
	expr_shutdown();
	clang_shutdown();
	return 0;
}

static void
test_parser_expr_impl(struct context *ctx, const char *src, const char *exp,
    int lno)
{
	struct buffer *bf;
	struct doc *concat, *expr;
	const char *act;
	int error;

	KS_expect_scope("parser_expr", lno, e);
	arena_scope(ctx->arena.eternal, eternal_scope);
	arena_scope(ctx->arena.buffer, buffer_scope);

	context_init(ctx, src, &eternal_scope);

	arena_scope(ctx->arena.doc, doc_scope);
	parser_arena_scope(&ctx->pr->pr_arena.doc_scope, &doc_scope, cookie);

	concat = doc_root(&doc_scope);
	error = parser_expr(ctx->pr, &expr, &(struct parser_expr_arg){
	    .dc		= concat,
	    .flags	= EXPR_EXEC_TEST,
	});
	KS_expect_int(GOOD, error);

	bf = arena_buffer_alloc(&buffer_scope, 1 << 8);
	doc_exec(&(struct doc_exec_arg){
	    .dc		= concat,
	    .scratch	= ctx->arena.scratch,
	    .bf		= bf,
	    .st		= ctx->st,
	});
	buffer_putc(bf, '\0');
	act = buffer_str(bf);
	KS_expect_str(exp, act);
}

static void
test_parser_type_impl(struct context *ctx, const char *src, const char *exp,
    unsigned int flags, int peek, int lno)
{
	const char *act;
	struct parser_type type;
	struct token *beg;
	int peek_act;

	KS_expect_scope("parser_type_peek", lno, e);
	arena_scope(ctx->arena.eternal, eternal_scope);
	arena_scope(ctx->arena.scratch, s);

	context_init(ctx, src, &eternal_scope);

	peek_act = parser_type_peek(ctx->pr, &type, flags);
	KS_expect_int(peek, peek_act);
	if (!peek)
		return;

	(void)lexer_peek(ctx->lx, &beg);
	act = tokens_concat(token_serialize_literal, beg, type.end, &s);
	KS_expect_true(act != NULL);
	KS_expect_str(exp, act);
}

static void
test_parser_attributes_peek_impl(struct context *ctx, const char *src,
    const char *exp, int peek, unsigned int flags, int lno)
{
	const char *act;
	struct token *beg, *rparen;
	int peek_act;

	KS_expect_scope("parser_attributes_peek", lno, e);
	arena_scope(ctx->arena.eternal, eternal_scope);
	arena_scope(ctx->arena.scratch, s);

	context_init(ctx, src, &eternal_scope);

	peek_act = parser_attributes_peek(ctx->pr, &rparen, flags);
	KS_expect_int(peek, peek_act);
	if (!peek)
		return;

	(void)lexer_peek(ctx->lx, &beg);
	act = tokens_concat(token_serialize_literal, beg, rparen, &s);
	KS_expect_true(act != NULL);
	KS_expect_str(exp, act);
}

static void
test_lexer_read_impl(struct context *ctx, const char *src, const char *exp,
    int lno)
{
	struct buffer *bf = NULL;
	const char *act;
	int ntokens = 0;

	KS_expect_scope("lexer_read", lno, e);
	arena_scope(ctx->arena.eternal, eternal_scope);
	arena_scope(ctx->arena.buffer, buffer_scope);
	arena_scope(ctx->arena.scratch, s);

	context_init(ctx, src, &eternal_scope);

	bf = arena_buffer_alloc(&buffer_scope, 128);
	for (;;) {
		struct token *tk;

		KS_expect_true(lexer_pop(ctx->lx, &tk));
		if (tk->tk_type == LEXER_EOF)
			break;

		if (ntokens++ > 0)
			buffer_putc(bf, ' ');
		buffer_printf(bf, "%s", token_serialize(tk, 0, &s));
	}
	act = buffer_str(bf);
	KS_expect_str(exp, act);
}

static void
test_token_position_after_impl(struct context *ctx,
    struct test_token_move *arg, int lno)
{
	struct token *after = NULL;
	struct token *move = NULL;
	int found;

	KS_expect_scope("token_position_after", lno, e);
	arena_scope(ctx->arena.eternal, eternal_scope);

	context_init(ctx, arg->src, &eternal_scope);

	found = find_token(ctx->lx, arg->target, &after);
	KS_expect_true(found);

	found = find_token(ctx->lx, arg->move, &move);
	KS_expect_true(found);

	token_position_after(after, move);
	assert_token_move(ctx, arg->want);
}

static int
find_token(struct lexer *lx, int type, struct token **tk)
{
	struct lexer_state s;
	struct token *discard;
	int found = 0;

	lexer_peek_enter(lx, &s);
	while (!lexer_if(lx, LEXER_EOF, NULL)) {
		if (lexer_if(lx, type, tk)) {
			found = 1;
			break;
		}
		if (!lexer_pop(lx, &discard))
			break;
	}
	lexer_peek_leave(lx, &s);
	return found;
}

static void
test_style_impl(struct context *ctx, const char *src, int key, int exp, int lno)
{
	struct style *st;
	int act;

	KS_expect_scope("style_parse", lno, e);
	arena_scope(ctx->arena.eternal, eternal_scope);

	context_init(ctx, src, &eternal_scope);
	options_trace_parse(&ctx->op, "ss");

	st = style_parse_buffer(ctx->bf, ".clang-format",
	    &eternal_scope, ctx->arena.scratch, &ctx->op);
	act = (int)style(st, key);
	KS_expect_int(exp, act);
}

static void
test_strwidth_impl(const char *str, size_t pos, size_t exp, int lno)
{
	size_t act;

	KS_expect_scope("strwidth", lno, e);

	act = strwidth(str, strlen(str), pos);
	KS_expect_int(exp, act);
}

static void
test_path_slice_impl(struct context *ctx, const char *path,
    unsigned int ncomponents, const char *exp, int lno)
{
	const char *act;

	KS_expect_scope("path_slice", lno, e);
	arena_scope(ctx->arena.scratch, s);

	act = path_slice(path, ncomponents, &s);
	KS_expect_str(exp, act);
}

static void
test_is_path_header_impl(const char *path, int exp, int lno)
{
	KS_expect_scope("path_slice", lno, e);

	KS_expect_int(exp, is_path_header(path));
}

static void
test_token_branch_impl(struct context *ctx)
{
	struct {
		int		 lno;
		const char	*src;
		int		 unlink;
		const char	*exp;
	} tests[] = {
		/* #if (unlink) -> #endif */
		{
			.lno	= __LINE__,
			.src	= "#if 0\n#endif\n",
			.unlink	= TOKEN_CPP_IF,
			.exp	= "CPP(#if 0\\n) CPP(#endif\\n)",
		},

		/* #if -> #endif (unlink) */
		{
			.lno	= __LINE__,
			.src	= "#if 0\n#endif\n",
			.unlink	= TOKEN_CPP_ENDIF,
			.exp	= "CPP(#if 0\\n) CPP(#endif\\n)",
		},

		/* #if (unlink) -> #else -> #endif */
		{
			.lno	= __LINE__,
			.src	= "#if 0\n#else\n#endif\n",
			.unlink	= TOKEN_CPP_IF,
			.exp	= "CPP(#if 0\\n) CPP_IF(#else\\n) CPP_ENDIF(#endif\\n)",
		},

		/* #if -> #else (unlink) -> #endif */
		{
			.lno	= __LINE__,
			.src	= "#if 0\n#else\n#endif\n",
			.unlink	= TOKEN_CPP_ELSE,
			.exp	= "CPP_IF(#if 0\\n) CPP(#else\\n) CPP_ENDIF(#endif\\n)",
		},

		/* #if -> #else -> #endif (unlink) */
		{
			.lno	= __LINE__,
			.src	= "#if 0\n#else\n#endif\n",
			.unlink	= TOKEN_CPP_ENDIF,
			.exp	= "CPP_IF(#if 0\\n) CPP_ENDIF(#else\\n) CPP(#endif\\n)",
		},

		/* #if -> #else -> #else (unlink) -> #endif */
		{
			.lno	= __LINE__,
			.src	= "#if 0\n#else\n#else\n#endif\n",
			.unlink	= TOKEN_CPP_ELSE,
			.exp	= "CPP_IF(#if 0\\n) CPP_ELSE(#else\\n) CPP(#else\\n) CPP_ENDIF(#endif\\n)",
		},
	};
	int ntests = sizeof(tests) / sizeof(tests[0]);
	int i;

	for (i = 0; i < ntests; i++) {
		struct token *it = NULL;
		struct token *prefix = NULL;
		struct token *tk;
		const char *act;

		KS_expect_scope("clang_token_branch_unlink", tests[i].lno, e);
		arena_scope(ctx->arena.eternal, eternal_scope);
		arena_scope(ctx->arena.scratch, s);

		context_init(ctx, tests[i].src, &eternal_scope);
		lexer_peek(ctx->lx, &tk);

		/* Find last prefix of the given type. */
		for (it = token_list_first(&tk->tk_prefixes);
		    it != NULL;
		    it = token_next(it)) {
			if (it->tk_type == tests[i].unlink)
				prefix = it;
		}
		KS_expect_true(prefix != NULL);

		clang_token_branch_unlink(prefix);
		act = tokens_concat(token_serialize_no_flags,
		    token_list_first(&tk->tk_prefixes), NULL, &s);
		KS_expect_str(tests[i].exp, act);
	}
}

static void
test_token_serialize(struct context *ctx)
{
	struct token *tkequal, *tkint;
	const char *act;

	arena_scope(ctx->arena.eternal, s);

	context_init(ctx, "int = ", &s);
	KS_expect_true(find_token(ctx->lx, TOKEN_INT, &tkint));
	KS_expect_true(find_token(ctx->lx, TOKEN_EQUAL, &tkequal));

	act = token_serialize(tkint, 0, &s);
	KS_expect_str("INT", act);

	act = token_serialize(tkint, TOKEN_SERIALIZE_VERBATIM, &s);
	KS_expect_str("INT(int)", act);

	act = token_serialize(tkint,
	    TOKEN_SERIALIZE_VERBATIM | TOKEN_SERIALIZE_QUOTE, &s);
	KS_expect_str("INT(\"int\")", act);

	act = token_serialize(tkint, TOKEN_SERIALIZE_POSITION, &s);
	KS_expect_str("INT<1:1>", act);
	act = token_serialize(tkequal,
	    TOKEN_SERIALIZE_POSITION | TOKEN_SERIALIZE_FLAGS, &s);
	KS_expect_str("EQUAL<1:5,ASSIGN>", act);

	act = token_serialize(tkint, TOKEN_SERIALIZE_FLAGS, &s);
	KS_expect_str("INT", act);
	act = token_serialize(tkequal, TOKEN_SERIALIZE_FLAGS, &s);
	KS_expect_str("EQUAL<ASSIGN>", act);

	act = token_serialize(tkint, TOKEN_SERIALIZE_REFS, &s);
	KS_expect_str("INT<1>", act);
}

static void
test_clang_token_serialize(struct context *ctx)
{
	struct token *tkerr;
	const char *act;

	arena_scope(ctx->arena.eternal, s);

	context_init(ctx, "err", &s);
	KS_expect_true(find_token(ctx->lx, TOKEN_IDENT, &tkerr));

	act = lexer_serialize(ctx->lx, tkerr);
	KS_expect_str("IDENT<1:1,ERR>(\"err\")", act);
}

static void
context_alloc(struct context *ctx)
{
	arenas_init(&ctx->arena);
	ctx->bf = NULL;
}

static void
context_free(struct context *ctx)
{
	if (ctx == NULL)
		return;

	arenas_free(&ctx->arena);
}

static void
context_init(struct context *ctx, const char *src,
    struct arena_scope *eternal_scope)
{
	static const char *path = "test.c";

	options_init(&ctx->op);

	ctx->bf = arena_buffer_alloc(eternal_scope, 128);
	buffer_puts(ctx->bf, src, strlen(src));

	ctx->st = style_parse("/dev/null", eternal_scope,
	    ctx->arena.scratch, &ctx->op);
	ctx->si = simple_alloc(eternal_scope, &ctx->op);
	ctx->cl = clang_alloc(ctx->st, ctx->si, &ctx->arena, &ctx->op,
	    eternal_scope);
	ctx->lx = lexer_tokenize(&(const struct lexer_arg){
	    .path		= path,
	    .bf			= ctx->bf,
	    .op			= &ctx->op,
	    .arena		= {
		.eternal_scope	= eternal_scope,
		.scratch	= ctx->arena.scratch,
	    },
	    .callbacks		= clang_lexer_callbacks(ctx->cl),
	});
	ctx->pr = parser_alloc(&(struct parser_arg){
	    .lexer	= ctx->lx,
	    .options	= &ctx->op,
	    .style	= ctx->st,
	    .simple	= ctx->si,
	    .arena	= {
		.scratch	= ctx->arena.scratch,
		.doc		= ctx->arena.doc,
	    },
	}, eternal_scope);
}

static void
assert_token_move(struct context *ctx, const char **want)
{
	struct lexer *lx = ctx->lx;
	const char *str;
	int i = 0;

	arena_scope(ctx->arena.scratch, s);

	for (;;) {
		struct token *prefix, *suffix, *tk;

		if (!lexer_pop(lx, &tk))
			break;

		LIST_FOREACH(prefix, &tk->tk_prefixes) {
			KS_expect_true(want[i] != NULL);

			str = token_serialize_position(&s, prefix);
			KS_expect_str(&want[i][2], str);
			i++;
		}

		if (tk->tk_type == LEXER_EOF)
			break;

		str = token_serialize_position(&s, tk);
		KS_expect_str(want[i], str);
		i++;

		LIST_FOREACH(suffix, &tk->tk_suffixes) {
			KS_expect_true(want[i] != NULL);

			str = token_serialize_position(&s, suffix);
			KS_expect_str(&want[i][2], str);
			i++;
		}
	}
	KS_expect_true(want[i] == NULL);
}

static const char *
tokens_concat(
    const char *(*serialize)(struct arena_scope *, const struct token *),
    const struct token *beg, const struct token *end, struct arena_scope *s)
{
	struct buffer *bf;
	const struct token *tk = beg;

	bf = arena_buffer_alloc(s, 128);

	for (;;) {
		const char *str;

		if (buffer_get_len(bf) > 0)
			buffer_putc(bf, ' ');
		str = serialize(s, tk);
		buffer_puts(bf, str, strlen(str));

		if (tk == end)
			break;
		tk = token_next(tk);
		if (tk == NULL)
			break;
	}

	return buffer_str(bf);
}

static const char *
token_serialize_literal(struct arena_scope *s, const struct token *tk)
{
	return arena_strndup(s, tk->tk_str, tk->tk_len);
}

static const char *
token_serialize_no_flags(struct arena_scope *s, const struct token *tk)
{
	return token_serialize(tk, TOKEN_SERIALIZE_VERBATIM, s);
}

static const char *
token_serialize_position(struct arena_scope *s, const struct token *tk)
{
	return token_serialize(tk,
	    TOKEN_SERIALIZE_VERBATIM | TOKEN_SERIALIZE_POSITION, s);
}
