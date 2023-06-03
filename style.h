struct options;
struct style;

/* Supported clang format options and values. */
enum {
	After = 1,
	AfterCaseLabel,
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
	Before,
	BeforeCatch,
	BeforeElse,
	BeforeLambdaBody,
	BeforeWhile,
	BitFieldColonSpacing,
	BlockIndent,
	Both,
	BraceWrapping,
	BreakBeforeBinaryOperators,
	BreakBeforeBraces,
	BreakBeforeTernaryOperators,
	CaseInsensitive,
	CaseSensitive,
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
	Priority,
	Regex,
	Right,
	SortIncludes,
	SortPriority,
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
void	style_shutdown(void);

struct style	*style_parse(const char *, const struct options *);
void		 style_free(struct style *);

unsigned int	style(const struct style *, int);
int		style_brace_wrapping(const struct style *, int);
