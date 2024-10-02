#include "options.h"	/* options_trace_level() */
#include "trace-types.h"

#define trace(trace, op, fmt, ...) do {					\
	if (options_trace_level((op), (trace)) > 0)			\
		trace_impl((trace), __func__, (fmt), __VA_ARGS__);	\
} while (0)

#define trace_no_func(trace, op, fmt, ...) do {				\
	if (options_trace_level((op), (trace)) > 0)			\
		trace_impl((trace), NULL, (fmt), __VA_ARGS__);		\
} while (0)

void	trace_impl(enum trace_type, const char *, const char *, ...)
	__attribute__((format(printf, 3, 4)));
