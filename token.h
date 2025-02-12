#include <stddef.h>	/* size_t */

#include "libks/list.h"

struct arena_scope;

#define FOR_TOKEN_TYPES(OP)						\
	/* keywords */							\
	OP(TOKEN_ASSEMBLY,	"asm", 0)				\
	OP(TOKEN_ATTRIBUTE,	"__attribute__", 0)			\
	OP(TOKEN_BREAK,		"break", 0)				\
	OP(TOKEN_CASE,		"case", 0)				\
	OP(TOKEN_CHAR,		"char", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_CONST,		"const", TOKEN_FLAG_QUALIFIER)		\
	OP(TOKEN_CONTINUE,	"continue", 0)				\
	OP(TOKEN_DEFAULT,	"default", 0)				\
	OP(TOKEN_DO,		"do", 0)				\
	OP(TOKEN_DOUBLE,	"double", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_ELSE,		"else", 0)				\
	OP(TOKEN_ENUM,		"enum", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_EXTERN,	"extern", TOKEN_FLAG_STORAGE)		\
	OP(TOKEN_FLOAT,		"float", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_FOR,		"for", 0)				\
	OP(TOKEN_GOTO,		"goto", 0)				\
	OP(TOKEN_IF,		"if", 0)				\
	OP(TOKEN_INLINE,	"inline", TOKEN_FLAG_STORAGE)		\
	OP(TOKEN_INT,		"int", TOKEN_FLAG_TYPE)			\
	OP(TOKEN_LONG,		"long", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_REGISTER,	"register", TOKEN_FLAG_STORAGE)		\
	OP(TOKEN_RESTRICT,	"restrict", TOKEN_FLAG_QUALIFIER)	\
	OP(TOKEN_RETURN,	"return", 0)				\
	OP(TOKEN_SHORT,		"short", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_SIGNED,	"signed", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_SIZEOF,	"sizeof", 0)				\
	OP(TOKEN_STATIC,	"static", TOKEN_FLAG_STORAGE)		\
	OP(TOKEN_STRUCT,	"struct", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_SWITCH,	"switch", 0)				\
	OP(TOKEN_TYPEDEF,	"typedef", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_UNION,		"union", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_UNSIGNED,	"unsigned", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_VOID,		"void", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_VOLATILE,	"volatile", TOKEN_FLAG_QUALIFIER)	\
	OP(TOKEN_WHILE,		"while", 0)				\
	/* punctuators */						\
	OP(TOKEN_LSQUARE,		"[", 0)				\
	OP(TOKEN_RSQUARE,		"]", 0)				\
	OP(TOKEN_LPAREN,		"(", 0)				\
	OP(TOKEN_RPAREN,		")", 0)				\
	OP(TOKEN_LBRACE,		"{", 0)				\
	OP(TOKEN_RBRACE,		"}", 0)				\
	OP(TOKEN_PERIOD,		".", TOKEN_FLAG_AMBIGUOUS)	\
	OP(TOKEN_ELLIPSIS,		"...", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_AMP,			"&", TOKEN_FLAG_AMBIGUOUS | TOKEN_FLAG_BINARY)\
	OP(TOKEN_AMPAMP,		"&&", TOKEN_FLAG_BINARY)	\
	OP(TOKEN_AMPEQUAL,		"&=", TOKEN_FLAG_ASSIGN)	\
	OP(TOKEN_STAR,			"*", TOKEN_FLAG_AMBIGUOUS | TOKEN_FLAG_BINARY | TOKEN_FLAG_SPACE)\
	OP(TOKEN_STAREQUAL,		"*=", TOKEN_FLAG_ASSIGN)	\
	OP(TOKEN_PLUS,			"+", TOKEN_FLAG_AMBIGUOUS | TOKEN_FLAG_BINARY)\
	OP(TOKEN_PLUSPLUS,		"++", 0)			\
	OP(TOKEN_PLUSEQUAL,		"+=", TOKEN_FLAG_ASSIGN)	\
	OP(TOKEN_MINUS,			"-", TOKEN_FLAG_AMBIGUOUS | TOKEN_FLAG_BINARY)\
	OP(TOKEN_ARROW,			"->", 0)			\
	OP(TOKEN_MINUSMINUS,		"--", 0)			\
	OP(TOKEN_MINUSEQUAL,		"-=", TOKEN_FLAG_ASSIGN)	\
	OP(TOKEN_TILDE,			"~", 0)				\
	OP(TOKEN_EXCLAIM,		"!", TOKEN_FLAG_AMBIGUOUS)	\
	OP(TOKEN_EXCLAIMEQUAL,		"!=", TOKEN_FLAG_BINARY)	\
	OP(TOKEN_SLASH,			"/", TOKEN_FLAG_AMBIGUOUS | TOKEN_FLAG_BINARY | TOKEN_FLAG_SPACE)\
	OP(TOKEN_SLASHEQUAL,		"/=", TOKEN_FLAG_ASSIGN)	\
	OP(TOKEN_PERCENT,		"%", TOKEN_FLAG_AMBIGUOUS | TOKEN_FLAG_BINARY)\
	OP(TOKEN_PERCENTEQUAL,		"%=", TOKEN_FLAG_ASSIGN)	\
	OP(TOKEN_LESS,			"<", TOKEN_FLAG_AMBIGUOUS | TOKEN_FLAG_BINARY)\
	OP(TOKEN_LESSLESS,		"<<", TOKEN_FLAG_AMBIGUOUS | TOKEN_FLAG_BINARY)\
	OP(TOKEN_LESSEQUAL,		"<=", TOKEN_FLAG_BINARY)	\
	OP(TOKEN_LESSLESSEQUAL,		"<<=", TOKEN_FLAG_ASSIGN)	\
	OP(TOKEN_GREATER,		">", TOKEN_FLAG_AMBIGUOUS | TOKEN_FLAG_BINARY)\
	OP(TOKEN_GREATERGREATER,	">>", TOKEN_FLAG_AMBIGUOUS | TOKEN_FLAG_BINARY)\
	OP(TOKEN_GREATEREQUAL,		">=", TOKEN_FLAG_BINARY)	\
	OP(TOKEN_GREATERGREATEREQUAL,	">>=", TOKEN_FLAG_ASSIGN)	\
	OP(TOKEN_CARET,			"^", TOKEN_FLAG_AMBIGUOUS)	\
	OP(TOKEN_CARETEQUAL,		"^=", TOKEN_FLAG_ASSIGN)	\
	OP(TOKEN_PIPE,			"|", TOKEN_FLAG_AMBIGUOUS | TOKEN_FLAG_BINARY | TOKEN_FLAG_SPACE)\
	OP(TOKEN_PIPEPIPE,		"||", TOKEN_FLAG_BINARY)	\
	OP(TOKEN_PIPEEQUAL,		"|=", TOKEN_FLAG_ASSIGN)	\
	OP(TOKEN_QUESTION,		"?", 0)				\
	OP(TOKEN_COLON,			":", 0)				\
	OP(TOKEN_SEMI,			";", 0)				\
	OP(TOKEN_EQUAL,			"=", TOKEN_FLAG_AMBIGUOUS | TOKEN_FLAG_ASSIGN)\
	OP(TOKEN_EQUALEQUAL,		"==", TOKEN_FLAG_BINARY)	\
	OP(TOKEN_COMMA,			",", 0)				\
	OP(TOKEN_BACKSLASH, 		"\\", TOKEN_FLAG_DISCARD)	\
	/* types */							\
	OP(TOKEN_BOOL,		"_Bool", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_INT8,		"int8_t", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_INT16,		"int16_t", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_INT32,		"int32_t", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_INT64,		"int64_t", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_UINT8,		"uint8_t", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_UINT16,	"uint16_t", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_UINT32,	"uint32_t", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_UINT64,	"uint64_t", TOKEN_FLAG_TYPE)		\
	OP(TOKEN_VA_LIST,	"va_list", TOKEN_FLAG_TYPE)

#define FOR_TOKEN_ALIASES(OP)						\
	/* assembly */							\
	OP(TOKEN_ASSEMBLY,	"__asm", 0)				\
	OP(TOKEN_ASSEMBLY,	"__asm_", 0)				\
	OP(TOKEN_ASSEMBLY,	"__asm__", 0)				\
	OP(TOKEN_ASSEMBLY,	"_asm", 0)				\
	OP(TOKEN_ASSEMBLY,	"_asm_", 0)				\
	OP(TOKEN_ASSEMBLY,	"asm_", 0)				\
	/* assembly linux */						\
	OP(TOKEN_ASSEMBLY,	"asm_inline", 0)			\
	OP(TOKEN_ASSEMBLY,	"asm_volatile_goto", 0)			\
	/* attribute */							\
	OP(TOKEN_ATTRIBUTE,	"__attribute", 0)			\
	OP(TOKEN_ATTRIBUTE,	"__attribute_", 0)			\
	OP(TOKEN_ATTRIBUTE,	"__attribute__", 0)			\
	OP(TOKEN_ATTRIBUTE,	"_attribute", 0)			\
	OP(TOKEN_ATTRIBUTE,	"_attribute_", 0)			\
	OP(TOKEN_ATTRIBUTE,	"_attribute__", 0)			\
	OP(TOKEN_ATTRIBUTE,	"attribute_", 0)			\
	OP(TOKEN_ATTRIBUTE,	"__declspec", 0)			\
	/* inline */							\
	OP(TOKEN_INLINE,	"_inline", TOKEN_FLAG_STORAGE)		\
	OP(TOKEN_INLINE,	"_inline_", TOKEN_FLAG_STORAGE)		\
	OP(TOKEN_INLINE,	"_inline__", TOKEN_FLAG_STORAGE)	\
	OP(TOKEN_INLINE,	"inline_", TOKEN_FLAG_STORAGE)		\
	/* restrict */							\
	OP(TOKEN_RESTRICT,	"_restrict", TOKEN_FLAG_QUALIFIER)	\
	/* volatile */							\
	OP(TOKEN_VOLATILE,	"__volatile", TOKEN_FLAG_QUALIFIER)	\
	OP(TOKEN_VOLATILE,	"__volatile__", TOKEN_FLAG_QUALIFIER)	\
	OP(TOKEN_VOLATILE,	"_volatile", TOKEN_FLAG_QUALIFIER)	\
	OP(TOKEN_VOLATILE,	"_volatile_", TOKEN_FLAG_QUALIFIER)	\
	/* BSD */							\
	OP(TOKEN_UINT8,		"u_int8_t", 0)				\
	OP(TOKEN_UINT16,	"u_int16_t", 0)				\
	OP(TOKEN_UINT32,	"u_int32_t", 0)				\
	OP(TOKEN_UINT64,	"u_int64_t", 0)

#define FOR_TOKEN_CPP(OP)						\
	/* type			normalized	keyword */		\
	OP(TOKEN_CPP_IF,	0,		"if")			\
	OP(TOKEN_CPP_IFDEF,	TOKEN_CPP_IF,	"ifdef")		\
	OP(TOKEN_CPP_IFNDEF,	TOKEN_CPP_IF,	"ifndef")		\
	OP(TOKEN_CPP_ELSE,	0,		"else")			\
	OP(TOKEN_CPP_ELIF,	TOKEN_CPP_ELSE,	"elif")			\
	OP(TOKEN_CPP_ENDIF,	0,		"endif")		\
	OP(TOKEN_CPP_DEFINE,	0,		"define")		\
	OP(TOKEN_CPP_INCLUDE,	0,		"include")

#define FOR_TOKEN_SENTINELS(OP)						\
	OP(TOKEN_COMMENT,	0)					\
	OP(TOKEN_CPP,		0)					\
	OP(TOKEN_IDENT,		0)					\
	OP(TOKEN_LITERAL,	0)					\
	OP(TOKEN_SPACE,		0)					\
	OP(TOKEN_STRING,	0)					\
	OP(TOKEN_NONE,		0)

#define TOKEN_SERIALIZE_VERBATIM	0x00000001u
#define TOKEN_SERIALIZE_POSITION	0x00000002u
#define TOKEN_SERIALIZE_FLAGS		0x00000004u
#define TOKEN_SERIALIZE_REFS		0x00000008u
#define TOKEN_SERIALIZE_ADDRESS		0x00000010u
#define TOKEN_SERIALIZE_QUOTE		0x00000020u

#define token_priv(tk, type) __extension__ ({				\
	typeof(tk) _nx = (tk) + 1;					\
	(type *)_nx;							\
})

enum {
#define OP(type, ...) type,
	FOR_TOKEN_TYPES(OP)
	FOR_TOKEN_CPP(OP)
	FOR_TOKEN_SENTINELS(OP)
#undef OP
};

LIST(token_list, token);

struct token {
	int			 tk_type;
	int			 tk_refs;
	unsigned int		 tk_priv_size;
	unsigned int		 tk_lno;
	unsigned int		 tk_cno;
	unsigned int		 tk_flags;
/* Token denotes a type keyword. */
#define TOKEN_FLAG_TYPE						0x00000001u
/* Token denotes a qualifier keyword. */
#define TOKEN_FLAG_QUALIFIER					0x00000002u
/* Token denotes a storage keyword. */
#define TOKEN_FLAG_STORAGE					0x00000004u
#define TOKEN_FLAG_BRANCH					0x00000008u
/* Token denotes an assignment operator. */
#define TOKEN_FLAG_ASSIGN					0x00000010u
#define TOKEN_FLAG_AMBIGUOUS					0x00000020u
#define TOKEN_FLAG_BINARY					0x00000040u
#define TOKEN_FLAG_DISCARD					0x00000080u
#define TOKEN_FLAG_COMMENT_C99					0x00000100u
#define TOKEN_FLAG_CPP						0x00000200u
/*
 * Token followed by exactly one new line. Dangling suffix and only emitted
 * in certain contexts.
 */
#define TOKEN_FLAG_OPTLINE					0x00000400u
/* Token followed by spaces or tabs. Dangling suffix and never emitted. */
#define TOKEN_FLAG_OPTSPACE					0x00000800u
/*
 * Token may be surrounded by spaces. Only applicable to binary operators, see
 * token.h.
 */
#define TOKEN_FLAG_SPACE					0x00001000u
/* Token covered by diff chunk. */
#define TOKEN_FLAG_DIFF						0x00002000u
#define TOKEN_FLAG_TYPE_FUNC					0x00004000u
#define TOKEN_FLAG_COMMENT_CLANG_FORMAT_OFF			0x00008000u
#define TOKEN_FLAG_COMMENT_CLANG_FORMAT_ON			0x00010000u

	const char		*tk_str;
	size_t			 tk_len;
	size_t			 tk_off;

	struct token_list	 tk_prefixes;
	struct token_list	 tk_suffixes;

	LIST_ENTRY(token_list, token);
};

struct token	*token_alloc(struct arena_scope *, unsigned int,
    const struct token *);
void		 token_ref(struct token *);
void		 token_rele(struct token *);
int		 token_trim(struct token *);
const char	*token_serialize(const struct token *, unsigned int,
    struct arena_scope *);
const char	*token_serialize_with_extra_flags(const struct token *,
    unsigned int, const char *, struct arena_scope *);

void	token_position_after(struct token *, struct token *);

int	token_cmp(const struct token *, const struct token *);
int	token_strcmp(const struct token *, const struct token *);
int	token_memcmp(const struct token *, const char *, size_t);
int	token_has_cpp(const struct token *);
int	token_type_normalize(const struct token *);
int	token_has_indent(const struct token *);
int	token_has_line(const struct token *, int);
int	token_has_verbatim_line(const struct token *, int);
int	token_has_prefixes(const struct token *);
int	token_has_suffix(const struct token *, int);
int	token_has_tabs(const struct token *);
int	token_has_spaces(const struct token *);
int	token_has_c99_comment(const struct token *);
int	token_is_decl(const struct token *, int);
int	token_is_moveable(const struct token *);
int	token_is_dangling(const struct token *);

#define token_next(tk)	__extension__ ({ (__typeof__(tk))LIST_NEXT(tk); })
#define token_prev(tk)	__extension__ ({ (__typeof__(tk))LIST_PREV(tk); })

unsigned int    token_lines(const struct token *);

void    token_set_str(struct token *, const char *, size_t);

void		 token_list_prepend(struct token_list *, struct token *);
void		 token_list_append(struct token_list *, struct token *);
void		 token_list_append_after(struct token_list *, struct token *,
    struct token *);
void		 token_list_remove(struct token_list *, struct token *);
void		 token_list_swap(struct token_list *, struct token_list *);
struct token	*token_list_first(struct token_list *);
struct token	*token_list_last(struct token_list *);
struct token	*token_list_find(const struct token_list *, int, unsigned int);

struct token	*token_find_suffix_spaces(struct token *);

void	token_move_suffixes(struct token *, struct token *);
void	token_move_suffixes_if(struct token *, struct token *, int);

unsigned int	token_flags_inherit(const struct token *);
