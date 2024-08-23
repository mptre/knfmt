#include "trace.h"

struct doc;
struct token;

/*
 * Return values for parser routines. Only one of the following may be returned
 * but disjoint values are favored allowing the caller to catch multiple return
 * values.
 */
#define GOOD	0x00000001
#define FAIL	0x00000002
#define NONE	0x00000004
#define BRCH	0x00000008
#define HALT	(FAIL | NONE | BRCH)

#define parser_trace(pr, fmt, ...) \
	trace(TRACE_PARSER, (pr)->pr_op, (fmt), __VA_ARGS__)

struct parser {
	const struct options	*pr_op;
	const struct style	*pr_st;
	struct simple		*pr_si;
	struct lexer		*pr_lx;
	struct clang		*pr_clang;
	unsigned int		 pr_nindent;	/* # indented stmt blocks */

	struct {
		struct arena_scope	*eternal_scope;
		struct arena		*scratch;
		struct arena_scope	*scratch_scope;
		struct arena		*doc;
		struct arena_scope	*doc_scope;
		struct arena		*buffer;
	} pr_arena;

	struct {
		struct simple_stmt		*stmt;
		struct simple_decl		*decl;
		struct simple_decl_forward	*decl_forward;
		struct simple_decl_proto	*decl_proto;
	} pr_simple;

	struct {
		struct ruler	*ruler;		/* align X macros */
	} pr_cpp;

	struct {
		struct token	*unmute;
		struct token	*clang_format_off;
	} pr_token;
};

struct parser_arena_scope_cookie {
	struct arena_scope	**restore_scope;
	struct arena_scope	 *old_scope;
};

int	parser_good(const struct parser *);
int	parser_none(const struct parser *);

#define parser_fail(a) \
	parser_fail_impl((a), __func__, __LINE__)
int	parser_fail_impl(struct parser *, const char *, int);

void	parser_reset(struct parser *);

#define parser_doc_token(a, b, c) \
	parser_doc_token_impl((a), (b), (c), __func__, __LINE__)
struct doc	*parser_doc_token_impl(struct parser *, struct token *,
    struct doc *, const char *, int);

void	parser_token_trim_after(const struct parser *, struct token *);
void	parser_token_trim_before(const struct parser *, struct token *);

int	parser_semi(struct parser *, struct doc *);

unsigned int	parser_width(struct parser *, const struct doc *);

int	parser_root(struct parser *, struct doc *);

#define parser_arena_scope(old_scope, new_scope, varname)		\
	__attribute__((cleanup(parser_arena_scope_leave)))		\
	    struct parser_arena_scope_cookie varname;			\
	parser_arena_scope_enter(&(varname), (old_scope), (new_scope))
void	parser_arena_scope_enter(struct parser_arena_scope_cookie *,
    struct arena_scope **, struct arena_scope *);
void	parser_arena_scope_leave(struct parser_arena_scope_cookie *);
