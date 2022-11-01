struct options;
struct style;

/* Supported clang format options and values. */
enum {
	AfterCaseLabel = 1,
	AfterClass,
	AfterControlStatement,
	AfterEnum,
	AfterExternBlock,
	AfterFunction,
	AfterNamespace,
	AfterObjCDeclaration,
	AfterStruct,
	AfterUnion,
	Align,
	AlignAfterOpenBracket,
	AlignAfterOperator,
	AlignEscapedNewlines,
	AlignOperands,
	AlignWithSpaces,
	All,
	AllDefinitions,
	Allman,
	Always,
	AlwaysBreak,
	AlwaysBreakAfterReturnType,
	Attach,
	BeforeCatch,
	BeforeElse,
	BeforeLambdaBody,
	BeforeWhile,
	BlockIndent,
	BraceWrapping,
	BreakBeforeBinaryOperators,
	BreakBeforeBraces,
	BreakBeforeTernaryOperators,
	ColumnLimit,
	ContinuationIndentWidth,
	Custom,
	DontAlign,
	False,
	ForContinuationAndIndentation,
	ForIndentation,
	GNU,
	IncludeCategories,
	IndentBraces,
	IndentWidth,
	Left,
	Linux,
	Mozilla,
	MultiLine,
	Never,
	NonAssignment,
	None,
	Right,
	SplitEmptyFunction,
	SplitEmptyNamespace,
	SplitEmptyRecord,
	Stroustrup,
	TopLevel,
	TopLevelDefinitions,
	True,
	UseTab,
	WebKit,
	Whitesmiths,

	Last,
};

void	style_init(void);
void	style_teardown(void);

struct style	*style_parse(const char *, const struct options *);
void		 style_free(struct style *);

unsigned int	style(const struct style *, int);
int		style_align(const struct style *);
int		style_brace_wrapping(const struct style *, int);
