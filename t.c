#include "config.h"

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libks/arena-buffer.h"
#include "libks/arena.h"
#include "libks/buffer.h"

#include "clang.h"
#include "doc.h"
#include "expr.h"
#include "fs.h"
#include "lexer-callbacks.h"
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

#ifdef HAVE_QUEUE
#  include <sys/queue.h>
#else
#  include "compat-queue.h"
#endif

struct context;

#define test(e) do {							\
	error |= (e);							\
	if (xflag && error) goto out;					\
	context_reset(&cx);						\
} while (0)

#define test_parser_expr(a, b) \
	test(test_parser_expr0(&cx, (a), (b), __LINE__))
static int	test_parser_expr0(struct context *, const char *, const char *,
    int);

#define test_parser_type_peek(a, b) \
	test(test_parser_type_peek0(&cx, (a), (b), 0, 1, __LINE__))
#define test_parser_type_peek_flags(a, b, c) \
	test(test_parser_type_peek0(&cx, (b), (c), (a), 1, __LINE__))
#define test_parser_type_peek_error(a) \
	test(test_parser_type_peek0(&cx, (a), "", 0, 0, __LINE__))
static int	test_parser_type_peek0(struct context *, const char *,
    const char *, unsigned int, int, int);

#define test_parser_attributes_peek(a, b) \
	test(test_parser_attributes_peek0(&cx, (a), (b), 1, 0, __LINE__))
#define test_parser_attributes_peek_flags(a, b, c) \
	test(test_parser_attributes_peek0(&cx, (b), (c), 1, (a), __LINE__))
#define test_parser_attributes_peek_flags_error(a, b) \
	test(test_parser_attributes_peek0(&cx, (b), "", 0, (a), __LINE__))
static int	test_parser_attributes_peek0(struct context *, const char *,
    const char *, int, unsigned int, int);

#define test_lexer_read(a, b) \
	test(test_lexer_read0(&cx, (a), (b), __LINE__))
static int	test_lexer_read0(struct context *, const char *, const char *,
    int);

struct test_token_move {
	const char	*src;
	int		 target;
	int		 move;
	const char	*want[16];
};

#define test_token_position_after(a) \
	test(test_token_position_after0(&cx, (a), __LINE__))
static int	test_token_position_after0(struct context *,
    struct test_token_move *, int);

#define test_style(a, b, c) \
	test(test_style0(&cx, (a), (b), (c), __LINE__))
static int	test_style0(struct context *, const char *, int, int, int);

#define test_strwidth(a, b, c) \
	test(test_strwidth0((a), (b), (c), __LINE__))
static int	test_strwidth0(const char *, size_t, size_t, int);

#define test_tmptemplate(a, b) \
	test(test_tmptemplate0(&cx, (a), (b), __LINE__))
static int	test_tmptemplate0(struct context *, const char *, const char *,
    int);

#define test_path_slice(a, b, c) \
	test(test_path_slice0((a), (b), (c), __LINE__))
static int	test_path_slice0(const char *, unsigned int, const char *, int);

#define test_token_branch_unlink() \
	test(test_token_branch_impl(&cx))
static int	test_token_branch_impl(struct context *);

struct context {
	struct options		 op;
	struct buffer		*bf;
	struct style		*st;
	struct simple		*si;
	struct clang		*cl;
	struct lexer		*lx;
	struct parser		*pr;

	struct {
		struct arena		*eternal;
		struct arena_scope	 eternal_scope;
		struct arena		*scratch;
		struct arena		*doc;
	} arena;
};

static void	usage(void) __attribute__((__noreturn__));

static void	context_alloc(struct context *);
static void	context_free(struct context *);
static void	context_init(struct context *, const char *);
static void	context_reset(struct context *);

static int	assert_token_move(struct context *, const char **, const char *,
    int);
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
main(int argc, char *argv[])
{
	struct context cx = {0};
	int error = 0;
	int xflag = 0;
	int ch;

	while ((ch = getopt(argc, argv, "x")) != -1) {
		switch (ch) {
		case 'x':
			xflag = 1;
			break;
		default:
			usage();
		}
	}

	clang_init();
	expr_init();
	style_init();
	context_alloc(&cx);

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

	test_parser_type_peek("void", "void");
	test_parser_type_peek("void *", "void *");
	test_parser_type_peek("void *p", "void *");
	test_parser_type_peek("size_t", "size_t");
	test_parser_type_peek("size_t s)", "size_t");
	test_parser_type_peek("size_t *", "size_t *");
	test_parser_type_peek("size_t *p", "size_t *");
	test_parser_type_peek("void main(int)", "void");
	test_parser_type_peek("void main(int);", "void");
	test_parser_type_peek("static foo void", "static foo void");
	test_parser_type_peek("static void foo", "static void foo");
	test_parser_type_peek("void foo f(void)", "void foo");
	test_parser_type_peek("char[]", "char [ ]");
	test_parser_type_peek(
	    "struct wsmouse_param[]",
	    "struct wsmouse_param [ ]");
	test_parser_type_peek("void)", "void");
	test_parser_type_peek("void,", "void");
	test_parser_type_peek("struct {,", "struct");
	test_parser_type_peek("struct s,", "struct s");
	test_parser_type_peek("struct s {,", "struct s");
	test_parser_type_peek("struct s *,", "struct s *");
	test_parser_type_peek("struct s **,", "struct s * *");
	test_parser_type_peek("struct s ***,", "struct s * * *");
	test_parser_type_peek("union {,", "union");
	test_parser_type_peek("union u,", "union u");
	test_parser_type_peek("union u {,", "union u");
	test_parser_type_peek("const* char*", "const * char *");
	test_parser_type_peek("char x[]", "char");
	test_parser_type_peek("va_list ap;", "va_list");
	test_parser_type_peek("struct s *M(v)", "struct s *");
	test_parser_type_peek(
	    "const struct filterops *const sysfilt_ops[]",
	    "const struct filterops * const");
	test_parser_type_peek("long __guard_local __attribute__", "long");
	test_parser_type_peek("unsigned int f:1", "unsigned int");
	test_parser_type_peek(
	    "usbd_status (*v)(void)",
	    "usbd_status ( * v ) ( void )");
	test_parser_type_peek("register char", "register char");
	test_parser_type_peek("...", "...");
	test_parser_type_peek("int (*f[])(void)", "int ( * f [ ] ) ( void )");
	test_parser_type_peek(
	    "int (* volatile f)(void);",
	    "int ( * volatile f ) ( void )");
	test_parser_type_peek(
	    "void (*const *f)(void);",
	    "void ( * const * f ) ( void )");
	test_parser_type_peek("int (**f)(void);", "int ( * * f ) ( void )");
	test_parser_type_peek("int (*f(void))(void)", "int");
	test_parser_type_peek(
	    "void (*f[1])(void)",
	    "void ( * f [ 1 ] ) ( void )");
	test_parser_type_peek("void (*)", "void ( * )");
	test_parser_type_peek("char (*v)[1]", "char ( * v ) [ 1 ]");
	test_parser_type_peek(
	    "P256_POINT (*table)[16]",
	    "P256_POINT ( * table ) [ 16 ]");
	test_parser_type_peek("char (*)[TP_BSIZE]", "char ( * ) [ TP_BSIZE ]");
	test_parser_type_peek(
	    "STACK_OF(X509_EXTENSION) x",
	    "STACK_OF ( X509_EXTENSION ) x");
	test_parser_type_peek(
	    "STACK_OF(X509_EXTENSION) *",
	    "STACK_OF ( X509_EXTENSION ) *");
	test_parser_type_peek(
	    "const STACK_OF(X509_EXTENSION)*",
	    "const STACK_OF ( X509_EXTENSION ) *");
	test_parser_type_peek(
	    "int\nmain(foo)\n\tfoo_t *",
	    "int");
	test_parser_type_peek("u_int:4", "u_int");

	test_parser_type_peek_flags(PARSER_TYPE_CAST,
	    "const foo_t)", "const foo_t");
	test_parser_type_peek_flags(PARSER_TYPE_ARG,
	    "const foo_t)", "const");
	test_parser_type_peek_flags(PARSER_TYPE_ARG,
	    "size_t)", "size_t");
	test_parser_type_peek_flags(PARSER_TYPE_ARG,
	    "foo_t[]", "foo_t [ ]");
	test_parser_type_peek_flags(PARSER_TYPE_EXPR,
	    "const foo_t)", "const foo_t");

	test_parser_type_peek_error("_asm volatile (");
	test_parser_type_peek_error("*");
	test_parser_type_peek_error("[");

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
	test_lexer_read("asm__", "ASSEMBLY");
	test_lexer_read("__asm__", "ASSEMBLY");
	test_lexer_read("__attribute", "ATTRIBUTE");
	test_lexer_read("attribute", "IDENT");

	test_lexer_read("asm_inline", "ASSEMBLY");
	test_lexer_read("asm_volatile_goto", "ASSEMBLY");

	test_token_position_after((&(struct test_token_move){
	    .src	= "\tint a;\n\tchar b;\n",
	    .target	= TOKEN_SEMI,
	    .move	= TOKEN_CHAR,
	    .want	= {
		"INT<1:9>(\"int\")",
		"  SPACE<1:12>(\" \")",
		"IDENT<1:13>(\"a\")",
		"SEMI<1:14>(\";\")",
		"  SPACE<1:15>(\"\\n\")",
		"CHAR<1:9>(\"char\")",
		"  SPACE<1:13>(\" \")",
		"IDENT<2:14>(\"b\")",
		"SEMI<2:15>(\";\")",
		"  SPACE<2:16>(\"\\n\")",
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

	test_tmptemplate("file.c", ".file.c.XXXXXXXX");
	test_tmptemplate("/file.c", "/.file.c.XXXXXXXX");
	test_tmptemplate("/root/file.c", "/root/.file.c.XXXXXXXX");

	test_path_slice("", 1, "");
	test_path_slice("", 2, "");
	test_path_slice("file", 1, "file");
	test_path_slice("file", 2, "file");
	test_path_slice("dir/file", 2, "dir/file");
	test_path_slice("/dir/file", 2, "dir/file");
	test_path_slice("dir/file", 3, "dir/file");

	test_token_branch_unlink();

out:
	context_free(&cx);
	style_shutdown();
	expr_shutdown();
	clang_shutdown();
	return error;
}

static void
usage(void)
{
	fprintf(stderr, "usage: t [-x]\n");
	exit(1);
}

static int
test_parser_expr0(struct context *cx, const char *src, const char *exp, int lno)
{
	struct buffer *bf = NULL;
	struct doc *concat, *expr;
	const char *act;
	int error;

	context_init(cx, src);

	arena_scope(cx->arena.doc, doc_scope);
	parser_arena_scope(&cx->pr->pr_arena.doc_scope, &doc_scope);

	concat = doc_root(&doc_scope);
	error = parser_expr(cx->pr, &expr, &(struct parser_expr_arg){
	    .dc	= concat,
	});
	if (error & HALT) {
		fprintf(stderr, "parser_expr:%d: want %x, got %x\n", lno,
		    (unsigned int)GOOD, (unsigned int)error);
		error = 1;
		goto out;
	}
	error = 0;

	bf = buffer_alloc(128);
	if (bf == NULL)
		err(1, NULL);
	doc_exec(&(struct doc_exec_arg){
	    .dc		= concat,
	    .scratch	= cx->arena.scratch,
	    .bf		= bf,
	    .st		= cx->st,
	    .op		= &cx->op,
	});
	buffer_putc(bf, '\0');
	act = buffer_get_ptr(bf);
	if (strcmp(exp, act) != 0) {
		fprintf(stderr, "parser_expr:%d:\n\texp \"%s\"\n\tgot \"%s\"\n",
		    lno, exp, act);
		error = 1;
	}

out:
	buffer_free(bf);
	return error;
}

static int
test_parser_type_peek0(struct context *cx, const char *src, const char *exp,
    unsigned int flags, int peek, int lno)
{
	const char *fun = "parser_type_peek";
	const char *act;
	struct parser_type type;
	struct token *beg;

	arena_scope(cx->arena.scratch, s);

	context_init(cx, src);

	if (parser_type_peek(cx->pr, &type, flags) != peek) {
		fprintf(stderr, "%s:%d: want %d, got %d\n",
		    fun, lno, peek, !peek);
		return 1;
	}
	if (!peek)
		return 0;

	(void)lexer_peek(cx->lx, &beg);
	act = tokens_concat(token_serialize_literal, beg, type.end, &s);
	if (act == NULL) {
		fprintf(stderr, "%s:%d: failed to concat tokens\n", fun, lno);
		return 1;
	}
	if (strcmp(exp, act) != 0) {
		fprintf(stderr, "%s:%d:\n"
		    "\texp \"%s\"\n\tgot \"%s\"\n",
		    fun, lno, exp, act);
		return 1;
	}

	return 0;
}

static int
test_parser_attributes_peek0(struct context *cx, const char *src,
    const char *exp, int peek, unsigned int flags, int lno)
{
	const char *fun = "parser_attributes_peek";
	const char *act;
	struct token *beg, *rparen;

	arena_scope(cx->arena.scratch, s);

	context_init(cx, src);

	if (parser_attributes_peek(cx->pr, &rparen, flags) != peek) {
		fprintf(stderr, "%s:%d: want %d, got %d\n",
		    fun, lno, peek, !peek);
		return 1;
	}
	if (!peek)
		return 0;

	(void)lexer_peek(cx->lx, &beg);
	act = tokens_concat(token_serialize_literal, beg, rparen, &s);
	if (act == NULL) {
		fprintf(stderr, "%s:%d: failed to concat tokens\n", fun, lno);
		return 1;
	}
	if (strcmp(exp, act) != 0) {
		fprintf(stderr, "%s:%d:\n"
		    "\texp \"%s\"\n\tgot \"%s\"\n",
		    fun, lno, exp, act);
		return 1;
	}

	return 0;
}

static int
test_lexer_read0(struct context *cx, const char *src, const char *exp,
    int lno)
{
	struct buffer *bf = NULL;
	const char *act;
	int error = 0;
	int ntokens = 0;

	context_init(cx, src);

	bf = buffer_alloc(128);
	if (bf == NULL)
		err(1, NULL);
	for (;;) {
		struct token *tk;
		const char *end, *str;
		size_t tokenlen;

		if (!lexer_pop(cx->lx, &tk))
			errx(1, "lexer_read:%d: out of tokens", lno);
		if (tk->tk_type == LEXER_EOF)
			break;

		if (ntokens++ > 0)
			buffer_putc(bf, ' ');
		str = lexer_serialize(cx->lx, tk);
		/* Strip of the token position and verbatim representation. */
		end = strchr(str, '<');
		tokenlen = (size_t)(end - str);
		buffer_puts(bf, str, tokenlen);
	}
	buffer_putc(bf, '\0');
	act = buffer_get_ptr(bf);
	if (strcmp(exp, act) != 0) {
		fprintf(stderr, "lexer_read:%d:\n\texp \"%s\"\n\tgot \"%s\"\n",
		    lno, exp, act);
		error = 1;
	}

	buffer_free(bf);
	return error;
}

static int
test_token_position_after0(struct context *cx,
    struct test_token_move *arg, int lno)
{
	const char *fun = "token_position_after";
	struct token *after, *move;
	int error = 0;

	context_init(cx, arg->src);

	if (!find_token(cx->lx, arg->target, &after)) {
		fprintf(stderr, "%s:%d: could not find after token\n",
		    fun, lno);
		return 1;
	}
	if (!find_token(cx->lx, arg->move, &move)) {
		fprintf(stderr, "%s:%d: could not find move token\n", fun, lno);
		return 1;
	}

	token_position_after(after, move);
	error = assert_token_move(cx, arg->want, fun, lno);

	return error;
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

static int
test_style0(struct context *cx, const char *src, int key, int exp, int lno)
{
	struct style *st;
	int error = 0;
	int act;

	context_init(cx, src);
	options_trace_parse(&cx->op, "ss");
	st = style_parse_buffer(cx->bf, ".clang-format",
	    &cx->arena.eternal_scope, cx->arena.scratch, &cx->op);
	act = (int)style(st, key);
	if (exp != act) {
		fprintf(stderr, "style_parse:%d:\n\texp %d\n\tgot %d\n",
		    lno, exp, act);
		error = 1;
	}
	return error;
}

static int
test_strwidth0(const char *str, size_t pos, size_t exp, int lno)
{
	size_t act;

	act = strwidth(str, strlen(str), pos);
	if (exp != act) {
		fprintf(stderr, "strwidth:%d:\n\texp %zu\n\tgot %zu\n",
		    lno, exp, act);
		return 1;
	}
	return 0;
}

static int
test_tmptemplate0(struct context *c, const char *path, const char *exp, int lno)
{
	char *act;
	int error = 0;

	arena_scope(c->arena.scratch, s);

	act = tmptemplate(path, &s);
	if (strcmp(act, exp) != 0) {
		const char *fun = "tmptemplate";

		fprintf(stderr, "%s:%d:\n\texp %s\n\tgot %s\n",
		    fun, lno, exp, act);
		error = 1;
	}
	return error;
}

static int
test_path_slice0(const char *path, unsigned int ncomponents, const char *exp,
    int lno)
{
	const char *act;

	act = path_slice(path, ncomponents);
	if (strcmp(act, exp) != 0) {
		const char *fun = "path_slice";

		fprintf(stderr, "%s:%d:\n\texp %s\n\tgot %s\n",
		    fun, lno, exp, act);
		return 1;
	}
	return 0;
}

static int
test_token_branch_impl(struct context *cx)
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
			.exp	= "CPP(\"#if 0\\n\") CPP(\"#endif\\n\")",
		},

		/* #if -> #endif (unlink) */
		{
			.lno	= __LINE__,
			.src	= "#if 0\n#endif\n",
			.unlink	= TOKEN_CPP_ENDIF,
			.exp	= "CPP(\"#if 0\\n\") CPP(\"#endif\\n\")",
		},

		/* #if (unlink) -> #else -> #endif */
		{
			.lno	= __LINE__,
			.src	= "#if 0\n#else\n#endif\n",
			.unlink	= TOKEN_CPP_IF,
			.exp	= "CPP(\"#if 0\\n\") CPP_IF(\"#else\\n\") CPP_ENDIF(\"#endif\\n\")",
		},

		/* #if -> #else (unlink) -> #endif */
		{
			.lno	= __LINE__,
			.src	= "#if 0\n#else\n#endif\n",
			.unlink	= TOKEN_CPP_ELSE,
			.exp	= "CPP_IF(\"#if 0\\n\") CPP(\"#else\\n\") CPP_ENDIF(\"#endif\\n\")",
		},

		/* #if -> #else -> #endif (unlink) */
		{
			.lno	= __LINE__,
			.src	= "#if 0\n#else\n#endif\n",
			.unlink	= TOKEN_CPP_ENDIF,
			.exp	= "CPP_IF(\"#if 0\\n\") CPP_ENDIF(\"#else\\n\") CPP(\"#endif\\n\")",
		},

		/* #if -> #else -> #else (unlink) -> #endif */
		{
			.lno	= __LINE__,
			.src	= "#if 0\n#else\n#else\n#endif\n",
			.unlink	= TOKEN_CPP_ELSE,
			.exp	= "CPP_IF(\"#if 0\\n\") CPP_ELSE(\"#else\\n\") CPP(\"#else\\n\") CPP_ENDIF(\"#endif\\n\")",
		},
	};
	int ntests = sizeof(tests) / sizeof(tests[0]);
	int error = 0;
	int i;

	for (i = 0; i < ntests; i++) {
		struct token *it = NULL;
		struct token *prefix = NULL;
		struct token *tk;
		const char *act;

		arena_scope(cx->arena.scratch, s);

		context_init(cx, tests[i].src);
		lexer_peek(cx->lx, &tk);

		/* Find last prefix of the given type. */
		for (it = token_list_first(&tk->tk_prefixes);
		    it != NULL;
		    it = token_next(it)) {
			if (it->tk_type == tests[i].unlink)
				prefix = it;
		}
		assert(prefix != NULL);

		clang_token_branch_unlink(prefix);
		act = tokens_concat(token_serialize_no_flags,
		    token_list_first(&tk->tk_prefixes), NULL, &s);
		if (strcmp(tests[i].exp, act) != 0) {
			fprintf(stderr, "%s:%d\n\texp \"%s\"\n\tgot \"%s\"\n",
			    __func__, tests[i].lno, tests[i].exp, act);
			error = 1;
		}
		context_reset(cx);
	}

	return error;
}

static void
context_alloc(struct context *cx)
{
	cx->arena.scratch = arena_alloc();
	cx->arena.eternal = arena_alloc();
	cx->arena.eternal_scope = arena_scope_enter(cx->arena.eternal);
	cx->arena.doc = arena_alloc();
	cx->bf = buffer_alloc(128);
	if (cx->bf == NULL)
		err(1, NULL);
	context_reset(cx);
}

static void
context_free(struct context *cx)
{
	if (cx == NULL)
		return;

	context_reset(cx);
	arena_scope_leave(&cx->arena.eternal_scope);
	buffer_free(cx->bf);
	arena_free(cx->arena.eternal);
	arena_free(cx->arena.scratch);
	arena_free(cx->arena.doc);
}

static void
context_init(struct context *cx, const char *src)
{
	static const char *path = "test.c";

	buffer_puts(cx->bf, src, strlen(src));
	cx->st = style_parse("/dev/null", &cx->arena.eternal_scope,
	    cx->arena.scratch, &cx->op);
	cx->si = simple_alloc(&cx->arena.eternal_scope, &cx->op);
	cx->cl = clang_alloc(cx->st, cx->si, &cx->arena.eternal_scope,
	    cx->arena.scratch, &cx->op);
	cx->lx = lexer_alloc(&(const struct lexer_arg){
	    .path		= path,
	    .bf			= cx->bf,
	    .eternal_scope	= &cx->arena.eternal_scope,
	    .op			= &cx->op,
	    .callbacks		= clang_lexer_callbacks(cx->cl),
	});
	cx->pr = parser_alloc(&(struct parser_arg){
	    .options	= &cx->op,
	    .style	= cx->st,
	    .simple	= cx->si,
	    .lexer	= cx->lx,
	    .arena	= {
		.eternal_scope	= &cx->arena.eternal_scope,
		.scratch	= cx->arena.scratch,
		.doc		= cx->arena.doc,
	    },
	});
}

static void
context_reset(struct context *cx)
{
	options_init(&cx->op);
	cx->op.test = 1;

	buffer_reset(cx->bf);
}

static int
assert_token_move(struct context *cx, const char **want, const char *fun,
    int lno)
{
	struct lexer *lx = cx->lx;
	const char *str;
	int i = 0;
	int error = 1;

	arena_scope(cx->arena.scratch, s);

	for (;;) {
		struct token *prefix, *suffix, *tk;

		if (!lexer_pop(lx, &tk))
			break;

		TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
			if (want[i] == NULL) {
				fprintf(stderr, "%s:%d: too few wanted "
				    "tokens\n", fun, lno);
				goto out;
			}

			str = token_serialize_position(&s, prefix);
			if (strcmp(str, &want[i][2]) != 0) {
				fprintf(stderr, "%s:%d: %d: want %s, got %s\n",
				    fun, lno, i, &want[i][2], str);
				goto out;
			}
			i++;
		}

		if (tk->tk_type == LEXER_EOF)
			break;

		str = token_serialize_position(&s, tk);
		if (strcmp(str, want[i]) != 0) {
			fprintf(stderr, "%s:%d: %d: want %s, got %s\n",
			    fun, lno, i, want[i], str);
			goto out;
		}
		i++;

		TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
			if (want[i] == NULL) {
				fprintf(stderr, "%s:%d: too few wanted "
				    "tokens\n", fun, lno);
				goto out;
			}

			str = token_serialize_position(&s, suffix);
			if (strcmp(str, &want[i][2]) != 0) {
				fprintf(stderr, "%s:%d: %d: want %s, got %s\n",
				    fun, lno, i, &want[i][2], str);
				goto out;
			}
			i++;
		}
	}
	if (want[i] != NULL) {
		fprintf(stderr, "%s:%d: wanted tokens not exhausted\n",
		    fun, lno);
		goto out;
	}
	error = 0;

out:
	return error;
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
	return token_serialize(tk, 0, s);
}

static const char *
token_serialize_position(struct arena_scope *s, const struct token *tk)
{
	return token_serialize(tk, TOKEN_SERIALIZE_POSITION, s);
}
