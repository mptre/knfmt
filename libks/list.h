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

#ifndef LIBKS_LIST_H
#define LIBKS_LIST_H

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)queue.h	8.5 (Berkeley) 8/20/94
 */

#define LIST(name, type)						\
struct name {								\
	struct type *tqh_first;						\
	struct type **tqh_last;						\
}

#define LIST_ENTRY(name, type)						\
struct {								\
	struct type *tqe_next;		/* next element */		\
	union {								\
		struct name *tqe_list;	/* used to access list type */	\
		struct type **tqe_prev;	/* address of previous next element */\
	};								\
} list_entry

#define LIST_FIELD(elm)			(elm)->list_entry

#define	LIST_FIRST(head)		((head)->tqh_first)

#define LIST_LAST(head) __extension__ ({				\
	__typeof__(head) _last = (__typeof__(head))(head)->tqh_last;	\
	*_last->tqh_last;						\
})

#define LIST_PREV(elm) __extension__ ({					\
	__typeof__(LIST_FIELD(elm).tqe_list) _last = (__typeof__(LIST_FIELD(elm).tqe_list))LIST_FIELD(elm).tqe_prev;\
	*_last->tqh_last;						\
})

#define	LIST_NEXT(elm)			(LIST_FIELD(elm).tqe_next)

#define	LIST_EMPTY(head)						\
	(LIST_FIRST(head) == NULL)

#define	LIST_LINKED(elm)						\
	(LIST_FIELD(elm).tqe_next != NULL || LIST_FIELD(elm).tqe_prev != NULL)

#define LIST_FOREACH(var, head)						\
	for((var) = LIST_FIRST(head);					\
	    (var) != NULL;						\
	    (var) = LIST_NEXT(var))

#define	LIST_FOREACH_SAFE(var, head, tvar)				\
	for ((var) = LIST_FIRST(head);					\
	    (var) != NULL &&						\
	    ((tvar) = LIST_NEXT(var), 1);				\
	    (var) = (tvar))

#define LIST_FOREACH_REVERSE(var, head)					\
	for((var) = LIST_LAST(head);					\
	    (var) != NULL;						\
	    (var) = LIST_PREV(var))

#define	LIST_FOREACH_REVERSE_SAFE(var, head, tvar)			\
	for ((var) = LIST_LAST(head);					\
	    (var) != NULL &&						\
	    ((tvar) = LIST_PREV(var), 1);				\
	    (var) = (tvar))

#define	LIST_INIT(head) do {						\
	(head)->tqh_first = NULL;					\
	(head)->tqh_last = &(head)->tqh_first;				\
} while (0)

#define LIST_INSERT_HEAD(head, elm) do {				\
	if ((LIST_FIELD(elm).tqe_next = (head)->tqh_first) != NULL)	\
		LIST_FIELD((head)->tqh_first).tqe_prev =		\
		    &LIST_FIELD(elm).tqe_next;				\
	else								\
		(head)->tqh_last = &LIST_FIELD(elm).tqe_next;		\
	(head)->tqh_first = (elm);					\
	LIST_FIELD(elm).tqe_prev = &(head)->tqh_first;			\
} while (0)

#define LIST_INSERT_TAIL(head, elm) do {				\
	LIST_FIELD(elm).tqe_next = NULL;				\
	LIST_FIELD(elm).tqe_prev = (head)->tqh_last;			\
	*(head)->tqh_last = (elm);					\
	(head)->tqh_last = &LIST_FIELD(elm).tqe_next;			\
} while (0)

#define LIST_INSERT_AFTER(head, listelm, elm) do {			\
	if ((LIST_FIELD(elm).tqe_next = LIST_FIELD(listelm).tqe_next) != NULL)\
		LIST_FIELD(LIST_FIELD(elm).tqe_next).tqe_prev =		\
		    &LIST_FIELD(elm).tqe_next;				\
	else								\
		(head)->tqh_last = &LIST_FIELD(elm).tqe_next;		\
	LIST_FIELD(listelm).tqe_next = (elm);				\
	LIST_FIELD(elm).tqe_prev = &LIST_FIELD(listelm).tqe_next;	\
} while (0)

#define	LIST_INSERT_BEFORE(listelm, elm) do {				\
	LIST_FIELD(elm).tqe_prev = LIST_FIELD(listelm).tqe_prev;	\
	LIST_FIELD(elm).tqe_next = (listelm);				\
	*LIST_FIELD(listelm).tqe_prev = (elm);				\
	LIST_FIELD(listelm).tqe_prev = &LIST_FIELD(elm).tqe_next;	\
} while (0)

#define LIST_REMOVE(head, elm) do {					\
	if ((LIST_FIELD(elm).tqe_next) != NULL)				\
		LIST_FIELD(LIST_FIELD(elm).tqe_next).tqe_prev =		\
		    LIST_FIELD(elm).tqe_prev;				\
	else								\
		(head)->tqh_last = LIST_FIELD(elm).tqe_prev;		\
	*LIST_FIELD(elm).tqe_prev = LIST_FIELD(elm).tqe_next;		\
	LIST_FIELD(elm).tqe_prev = NULL;				\
	LIST_FIELD(elm).tqe_next = NULL;				\
} while (0)

#define LIST_CONCAT(head1, head2) do {					\
	if (!LIST_EMPTY(head2)) {					\
		*(head1)->tqh_last = (head2)->tqh_first;		\
		LIST_FIELD((head2)->tqh_first).tqe_prev = (head1)->tqh_last;\
		(head1)->tqh_last = (head2)->tqh_last;			\
		LIST_INIT((head2));					\
	}								\
} while (0)

#endif /* !LIBKS_LIST_H */
