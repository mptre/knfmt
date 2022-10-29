struct options;
struct style;

/* Supported clang format options and values. */
enum {
	Align = 1,
	AlignAfterOpenBracket,
	AlignAfterOperator,
	AlignEscapedNewlines,
	AlignOperands,
	AlignWithSpaces,
	All,
	AllDefinitions,
	Always,
	AlwaysBreak,
	AlwaysBreakAfterReturnType,
	BlockIndent,
	BraceWrapping,
	BreakBeforeBinaryOperators,
	BreakBeforeTernaryOperators,
	ColumnLimit,
	ContinuationIndentWidth,
	DontAlign,
	False,
	ForContinuationAndIndentation,
	ForIndentation,
	IncludeCategories,
	IndentWidth,
	Left,
	Never,
	NonAssignment,
	None,
	Right,
	TopLevel,
	TopLevelDefinitions,
	True,
	UseTab,

	Last,
};

void	style_init(void);
void	style_teardown(void);

struct style	*style_parse(const char *, const struct options *);
void		 style_free(struct style *);

unsigned int	style(const struct style *, int);
int		style_align(const struct style *);
