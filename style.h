struct options;
struct style;

/* Supported clang format options and values. */
#define FOR_STYLES(OP)							\
	OP(After)							\
	OP(Align)							\
	OP(AlignAfterOpenBracket)					\
	OP(AlignAfterOperator)						\
	OP(AlignEscapedNewlines)					\
	OP(AlignOperands)						\
	OP(AlignWithSpaces)						\
	OP(All)								\
	OP(AllDefinitions)						\
	OP(Allman)							\
	OP(Always)							\
	OP(AlwaysBreak)							\
	OP(AlwaysBreakAfterReturnType)					\
	OP(Attach)							\
	OP(Before)							\
	OP(BitFieldColonSpacing)					\
	OP(BlockIndent)							\
	OP(Both)							\
	OP(BraceWrapping)						\
	  OP(AfterCaseLabel)						\
	  OP(AfterClass)						\
	  OP(AfterControlStatement)					\
	  OP(AfterEnum)							\
	  OP(AfterExternBlock)						\
	  OP(AfterFunction)						\
	  OP(AfterNamespace)						\
	  OP(AfterObjCDeclaration)					\
	  OP(AfterStruct)						\
	  OP(AfterUnion)						\
	  OP(BeforeCatch)						\
	  OP(BeforeElse)						\
	  OP(BeforeLambdaBody)						\
	  OP(BeforeWhile)						\
	  OP(IndentBraces)						\
	  OP(SplitEmptyFunction)					\
	  OP(SplitEmptyNamespace)					\
	  OP(SplitEmptyRecord)						\
	OP(BreakBeforeBinaryOperators)					\
	OP(BreakBeforeBraces)						\
	OP(BreakBeforeTernaryOperators)					\
	OP(CaseInsensitive)						\
	OP(CaseSensitive)						\
	OP(ColumnLimit)							\
	OP(ContinuationIndentWidth)					\
	OP(Custom)							\
	OP(DontAlign)							\
	OP(False)							\
	OP(ForContinuationAndIndentation)				\
	OP(ForIndentation)						\
	OP(GNU)								\
	OP(IncludeCategories)						\
	OP(IndentWidth)							\
	OP(Left)							\
	OP(Linux)							\
	OP(Mozilla)							\
	OP(MultiLine)							\
	OP(Never)							\
	OP(NonAssignment)						\
	OP(None)							\
	OP(Priority)							\
	OP(Regex)							\
	OP(Right)							\
	OP(SortIncludes)						\
	OP(SortPriority)						\
	OP(Stroustrup)							\
	OP(TopLevel)							\
	OP(TopLevelDefinitions)						\
	OP(True)							\
	OP(UseTab)							\
	OP(WebKit)							\
	OP(Whitesmiths)

enum style_keyword {
	First = 1,
#define DO(style) style,
	FOR_STYLES(DO)
#undef DO
	Last,
};

void	style_init(void);
void	style_shutdown(void);

struct style	*style_parse(const char *, const struct options *);
void		 style_free(struct style *);

unsigned int	style(const struct style *, int);
int		style_brace_wrapping(const struct style *, int);
