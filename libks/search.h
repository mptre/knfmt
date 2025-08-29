/*
 * Copyright (c) 2025 Anton Lindqvist <anton@basename.se>
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

#ifndef LIBKS_SEARCH_H
#define LIBKS_SEARCH_H

#define KS_binary_search(v, n, cmp, needle) __extension__ ({		\
	__typeof__(*(v)) *_out = NULL;					\
	if ((n) > 0) {							\
		size_t _l = 0;						\
		size_t _r = (size_t)(n) - 1;				\
		while (_l <= _r) {					\
			size_t _m = (_r + _l) / 2;			\
			int _c = (cmp)(&(v)[_m], (needle));		\
			if (_c == 0) {					\
				_out = &(v)[_m];			\
				break;					\
			} else if (_c < 0) {				\
				_l = _m + 1;				\
			} else if (_c > 0) {				\
				_r = _m - 1;				\
			}						\
		}							\
	}								\
	_out;								\
})

#endif /* !LIBKS_SEARCH_H */
