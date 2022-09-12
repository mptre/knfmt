struct options;
struct style;

/* Supported clang format options and values. */
enum {
	AlignAfterOpenBracket,
	Align,
	DontAlign,
	AlwaysBreak,
	BlockIndent,

	AlwaysBreakAfterReturnType,
	None,
	All,
	TopLevel,
	AllDefinitions,
	TopLevelDefinitions,

	BraceWrapping,

	ColumnLimit,
/*	Integer,	*/

	ContinuationIndentWidth,
/*	Integer,	*/

	IncludeCategories,

	IndentWidth,
/*	Integer,	*/

	UseTab,
	Never,
	ForIndentation,
	ForContinuationAndIndentation,
	AlignWithSpaces,
	Always,

	Last,
};

void	style_init(void);
void	style_teardown(void);

struct style	*style_parse(const struct options *);
void		 style_free(struct style *);

unsigned int	style(const struct style *, int);
