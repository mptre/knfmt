/*
 * Copyright (c) 2024 Anton Lindqvist <anton@basename.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef LIBKS_EXPECT_H
#define LIBKS_EXPECT_H

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct KS_expect_scope {
	const char *fun;
	int         lno;
};

static struct KS_expect_scope KS_expect_current_scope;

__attribute__((used))
static void
KS_expect_scope_leave(void *arg)
{
	const struct KS_expect_scope *scope = arg;

	KS_expect_current_scope = *scope;
}

#define KS_expect_scope(f, l, varname)					\
	__attribute__((cleanup(KS_expect_scope_leave)))			\
	struct KS_expect_scope varname = KS_expect_current_scope;	\
	(void)(varname); /* avoid unused warnings */			\
	KS_expect_current_scope.fun = (f);				\
	KS_expect_current_scope.lno = (l)

#define KS_expect_func()						\
	(KS_expect_current_scope.fun != NULL ? KS_expect_current_scope.fun :\
	 __func__)
#define KS_expect_line()						\
	(KS_expect_current_scope.lno != 0 ? KS_expect_current_scope.lno :\
	 __LINE__)

#define KS_expect_true(expr) KS_expect_int(1, (expr) ? 1 : 0)
#define KS_expect_false(expr) KS_expect_int(0, (expr))

#define KS_expect_int(exp, act) __extension__ ({			\
	__typeof__(exp) _exp = (exp);					\
	__typeof__(act) _act = (act);					\
	union {								\
		int64_t i64;						\
		uint64_t u64;						\
	} _e, _a;							\
	__builtin_choose_expr((__typeof__(_exp))-1 > 0,			\
	    _e.i64 = (int64_t)_exp, _e.u64 = (uint64_t)_exp);		\
	__builtin_choose_expr((__typeof__(_act))-1 > 0,			\
	    _a.i64 = (int64_t)_act, _a.u64 = (uint64_t)_act);		\
	if (memcmp(&_a.u64, &_e.u64, sizeof(uint64_t)) != 0) {		\
		fprintf(stderr, "%s:%d:\n"				\
		    "expected: %"PRId64" (0x%"PRIx64")\n"		\
		    "  actual: %"PRId64" (0x%"PRIx64")\n",		\
		    KS_expect_func(), KS_expect_line(),			\
		    _e.i64, _e.u64, _a.i64, _a.u64);			\
		exit(1);						\
	}								\
})

#define KS_expect_str(exp, act) \
	KS_expect_str_n((exp), (act), strlen((act)))

#define KS_expect_str_n(exp, act, act_len) __extension__ ({		\
	typeof(exp) _exp = (exp);					\
	typeof(act) _act = (act);					\
	size_t _exp_len = strlen(_exp);					\
	size_t _act_len = (act_len);					\
	if (_exp_len != _act_len ||					\
	    strncmp(_exp, _act, _act_len) != 0) {			\
		fprintf(stderr, "%s:%d:\n"				\
		    "expected: \"%.*s\"\n"				\
		    "  actual: \"%.*s\"\n",				\
		    KS_expect_func(), KS_expect_line(),			\
		    (int)_exp_len, _exp, (int)_act_len, _act);		\
		exit(1);						\
	}								\
})

#define KS_expect_ptr(exp, act) __extension__ ({			\
	uintptr_t _exp = (uintptr_t)(exp);				\
	uintptr_t _act = (uintptr_t)(act);				\
	if (_exp != _act) {						\
		fprintf(stderr, "%s:%d:\n"				\
		    "expected: 0x%"PRIxPTR"\n"				\
		    "  actual: 0x%"PRIxPTR"\n",				\
		    KS_expect_func(), KS_expect_line(),			\
		    _exp, _act);					\
		exit(1);						\
	}								\
})

#endif /* !LIBKS_EXPECT_H */
