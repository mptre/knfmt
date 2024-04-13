struct arena;
struct arena_scope;
struct buffer;
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
	OP(Always)							\
	OP(AlwaysBreak)							\
	OP(AlwaysBreakAfterReturnType)					\
	OP(BasedOnStyle)						\
	  OP(Chromium)							\
	  OP(Google)							\
	  OP(InheritParentConfig)					\
	  OP(LLVM)							\
	  OP(Microsoft)							\
	  OP(OpenBSD)							\
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
	  OP(Allman)							\
	  OP(Attach)							\
	  OP(Custom)							\
	  OP(GNU)							\
	  OP(Linux)							\
	  OP(Mozilla)							\
	  OP(Stroustrup)						\
	  OP(WebKit)							\
	  OP(Whitesmiths)						\
	OP(BreakBeforeBraces)						\
	OP(BreakBeforeTernaryOperators)					\
	OP(CaseInsensitive)						\
	OP(CaseSensitive)						\
	OP(ColumnLimit)							\
	OP(ContinuationIndentWidth)					\
	OP(DontAlign)							\
	OP(False)							\
	OP(ForContinuationAndIndentation)				\
	OP(ForIndentation)						\
	OP(IncludeBlocks)						\
	  OP(Merge)							\
	  OP(Preserve)							\
	  OP(Regroup)							\
	OP(IncludeCategories)						\
	OP(IncludeGuards)						\
	OP(IndentWidth)							\
	OP(Language)							\
	  OP(Cpp)							\
	  OP(CSharp)							\
	  OP(Java)							\
	  OP(JavaScript)						\
	  OP(Json)							\
	  OP(ObjC)							\
	  OP(Proto)							\
	  OP(TableGen)							\
	  OP(TextProto)							\
	  OP(Verilog)							\
	OP(Left)							\
	OP(MultiLine)							\
	OP(Never)							\
	OP(NonAssignment)						\
	OP(None)							\
	OP(Priority)							\
	OP(Regex)							\
	OP(Right)							\
	OP(SortIncludes)						\
	OP(SortPriority)						\
	OP(TopLevel)							\
	OP(TopLevelDefinitions)						\
	OP(True)							\
	OP(UseTab)

enum style_keyword {
	First = 1,
#define DO(style) style,
	FOR_STYLES(DO)
#undef DO
	Last,
};

struct include_priority {
	int	group;
	int	sort;
};

void	style_init(void);
void	style_shutdown(void);
void	style_dump_keywords(struct buffer *);

struct style	*style_parse(const char *, struct arena_scope *,
    struct arena *, const struct options *);
struct style	*style_parse_buffer(const struct buffer *, const char *,
    struct arena_scope *, struct arena *, const struct options *);

unsigned int	style(const struct style *, int);
int		style_brace_wrapping(const struct style *, int);

int			*style_include_priorities(const struct style *);
struct include_priority	 style_include_priority(const struct style *,
    const char *);

const char	*style_keyword_str(enum style_keyword);

static inline int
style_use_tabs(const struct style *st)
{
	return style(st, UseTab) != Never;
}
