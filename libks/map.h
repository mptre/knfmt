/*
 * Copyright (c) 2023 Anton Lindqvist <anton@basename.se>
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

#ifndef LIBKS_MAP_H
#define LIBKS_MAP_H

#include <stddef.h>     /* size_t */

#define MAP_KEY_PTR		0x00000001u
#define MAP_KEY_STR		0x00000002u

#define MAP(key, ptr, val) struct {					\
	char ptr	 p;						\
	key		 k;						\
	key ptr		 kp;						\
	val		*v; /* NOLINT(bugprone-macro-parentheses) */	\
} *

#define MAP_INIT(m) __extension__ ({					\
	unsigned int _flags = 0;					\
	if (sizeof((m)->p) > sizeof(char))				\
		_flags |= MAP_KEY_PTR;					\
	if (__builtin_types_compatible_p(__typeof__((m)->kp), char *) ||\
	    __builtin_types_compatible_p(__typeof__((m)->kp), const char *))\
		_flags |= MAP_KEY_STR;					\
	map_init((void **)&(m), sizeof((m)->k), sizeof(*(m)->v), _flags);\
})
int	map_init(void **, size_t, size_t, unsigned int);

#define MAP_FREE(m) map_free((void *)(m))
void	map_free(void *);

#define MAP_INSERT(m, key) __extension__ ({				\
	__typeof__((m)->kp) _k = (key);					\
	(__typeof__((m)->v))map_insert((m), (const void *const *)&_k);	\
})
void	*map_insert(void *, const void *const *);

#define MAP_INSERT_N(m, key, keysize) __extension__ ({			\
	__typeof__((m)->kp) _k = (key);					\
	(__typeof__((m)->v))map_insert_n((m),				\
	    (const void *const *)&_k, (keysize));			\
})
void	*map_insert_n(void *, const void *const *, size_t);

#define MAP_INSERT_VALUE(m, key, val) __extension__ ({			\
	__typeof__((m)->v) _e = MAP_INSERT((m), (key));			\
	if (_e != NULL)							\
		*_e = (const __typeof__(*(m)->v))(val);			\
	_e;								\
})

#define MAP_KEY(m, val) __extension__ ({				\
	const __typeof__(*(m)->v) *_v = (val);				\
	(const __typeof__((m)->k) *)map_key((m), _v);			\
})
void	*map_key(void *, const void *);

#define MAP_FIND(m, key) __extension__ ({				\
	const __typeof__((m)->kp) _k = (key);				\
	(__typeof__((m)->v))map_find((m), (const void *const *)&_k);	\
})
void	*map_find(void *, const void *const *);

#define MAP_FIND_N(m, key, keysize) __extension__ ({			\
	const __typeof__((m)->kp) _k = (key);				\
	(__typeof__((m)->v))map_find_n((m),				\
	    (const void *const *)&_k, (keysize));			\
})
void	*map_find_n(void *, const void *const *, size_t);

#define MAP_REMOVE(m, key) do {						\
	const __typeof__((m)->kp) _k = (key);				\
	map_remove((m), (const void *const *)&_k);			\
} while (0)
void	map_remove(void *, const void *const *);

struct map_iterator {
	void	*el;
	void	*nx;
};

#define MAP_ITERATOR(m) struct {					\
	struct map_iterator	it;					\
	__typeof__((m)->kp)	key;					\
	__typeof__((m)->v)	val;					\
}

#define MAP_ITERATE(m, iterator) __extension__ ({			\
	(iterator)->val = (__typeof__((m)->v))map_iterate(		\
	    (m), &((iterator)->it));					\
	if ((iterator)->val != NULL) {					\
		void *_k = map_key((m), (iterator)->val);		\
		(iterator)->key = __builtin_choose_expr(		\
		    sizeof((m)->p) > sizeof(char),			\
		    _k,							\
		    *((__typeof__((m)->k) *)_k));			\
	}								\
	/* Suppress cppcheck unreadVariable false positives. */		\
	(void)(iterator)->key;						\
	(void)(iterator)->val;						\
	(iterator)->val != NULL ? 1 : 0;				\
})
void	*map_iterate(void *, struct map_iterator *);

#endif /* !LIBKS_MAP_H */
