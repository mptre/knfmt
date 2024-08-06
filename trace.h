#ifndef TRACE_H
#define TRACE_H

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

#include "options.h"	/* options_trace_level() */

#define trace(trace, op, fmt, ...) do {					\
	if (options_trace_level((op), (trace)) > 0)			\
		trace_impl((trace), __func__, (fmt), __VA_ARGS__);	\
} while (0)

#define trace_no_func(trace, op, fmt, ...) do {				\
	if (options_trace_level((op), (trace)) > 0)			\
		trace_impl((trace), NULL, (fmt), __VA_ARGS__);		\
} while (0)

void	trace_impl(enum trace_type, const char *, const char *, ...)
	__attribute__((__format__(printf, 3, 4)));

#endif
