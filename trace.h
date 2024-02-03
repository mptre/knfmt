#include "options.h"	/* is_trace_enabled() */

#define trace(trace, op, fmt, ...) do {					\
	if (options_trace_level((op), (trace)) > 0)			\
		trace_impl((trace), __func__, (fmt), __VA_ARGS__);	\
} while (0)

#define trace_no_func(trace, op, fmt, ...) do {				\
	if (options_trace_level((op), (trace)) > 0)			\
		trace_impl((trace), NULL, (fmt), __VA_ARGS__);		\
} while (0)

void	trace_impl(char, const char *, const char *, ...)
	__attribute__((__format__(printf, 3, 4)));
