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

#ifndef LIBKS_SECTION_H
#define LIBKS_SECTION_H

#if defined(__MACH__)
/* Prevent Clang on macOS from adding poisoned bytes between section entries. */
#  define SECTION(s)		__attribute__((section("__DATA," #s))) \
				__attribute__((no_sanitize("address")))
#  define SECTION_START(s)	__asm("section$start$__DATA$" #s);
#  define SECTION_STOP(s)	__asm("section$end$__DATA$" #s);
#else
#  define SECTION(s)		__attribute__((section(#s)))
#  define SECTION_START(s)
#  define SECTION_STOP(s)
#endif

#define SECTION_ITERATE(it, s) __extension__ ({				\
	_Static_assert((sizeof(*(it)) & 0xf) == 0, "Unaligned");	\
	extern __typeof__(*(it)) __start_ ## s SECTION_START(s);	\
	__typeof__(it) _start = &__start_ ## s;				\
	extern __typeof__(*(it)) __stop_##s SECTION_STOP(s);		\
	__typeof__(it) _stop = &__stop_ ## s;				\
	if ((it) == NULL)						\
		(it) = _start;						\
	else								\
		(it)++;							\
	(it) != _stop;							\
})

#endif /* !LIBKS_SECTION_H */
