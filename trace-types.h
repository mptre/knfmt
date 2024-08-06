#ifndef TRACE_TYPES_H
#define TRACE_TYPES_H

#define FOR_TRACE_TYPES(OP)		\
	OP(ALL,			'a')	\
	OP(CLANG,		'c')	\
	OP(CPP,			'C')	\
	OP(DOC,			'd')	\
	OP(DIFF,		'D')	\
	OP(FUNC,		'f')	\
	OP(LEXER,		'l')	\
	OP(PARSER,		'p')	\
	OP(STYLE,		's')	\
	OP(SIMPLE,		'S')	\
	OP(TOKEN,		't')

enum trace_type {
#define OP(name, ...) TRACE_ ## name,
	FOR_TRACE_TYPES(OP)
#undef OP
	TRACE_MAX,
};

#endif
