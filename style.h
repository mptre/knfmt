struct options;
struct style;

/* Supported clang format options and values. */
enum {
	AlwaysBreakAfterReturnType,
	None,
	All,
	TopLevel,
	AllDefinitions,
	TopLevelDefinitions,

	ColumnLimit,
/*	Integer,	*/

	ContinuationIndentWidth,
/*	Integer,	*/

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
