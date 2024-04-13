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

#include "libks/arena.h"

#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libks/compiler.h"

#if defined(__has_feature)
#  if  __has_feature(address_sanitizer)
#    define HAVE_ASAN 1	/* clang */
#  endif
#elif defined(__SANITIZE_ADDRESS__)
#  define HAVE_ASAN 1	/ * gcc */
#endif
#if defined(HAVE_ASAN)
#  include <sanitizer/asan_interface.h>
#  define POISON_SIZE 8
#else
#  define ASAN_POISON_MEMORY_REGION(...) (void)0
#  define ASAN_UNPOISON_MEMORY_REGION(...) (void)0
#  define POISON_SIZE 0
#endif

#define MAX_SOURCE_LOCATIONS 8

struct source_location {
	const char	*fun;
	int		 lno;
};

struct arena_cleanup {
	void			*ptr;
	void			 (*fun)(void *);
	struct arena_cleanup	*next;
};

struct arena_frame {
	char			*ptr;
	size_t			 size;
	size_t			 len;
	struct arena_frame	*next;
};

struct arena {
	struct arena_frame	*frame;
	/* Initial heap frame size, multiple of page size. */
	size_t			 frame_size;
	/* Number of ASAN poison bytes between allocations. */
	size_t			 poison_size;
	int			 refs;
	struct arena_stats	 stats;
	struct source_location	 scope_locations[MAX_SOURCE_LOCATIONS];
};

union address {
	char		*s8;
	uint64_t	 u64;
};

static const size_t maxalign = sizeof(void *);

static void
frame_poison(struct arena_frame *MAYBE_UNUSED(frame))
{
	ASAN_POISON_MEMORY_REGION(&frame->ptr[frame->len],
	    frame->size - frame->len);
}

static void
frame_unpoison(struct arena_frame *MAYBE_UNUSED(frame),
    size_t MAYBE_UNUSED(size))
{
	ASAN_UNPOISON_MEMORY_REGION(&frame->ptr[frame->len], size);
}

static union address
align_address(const struct arena *a, union address addr)
{
	addr.u64 = (addr.u64 + maxalign - 1) & ~(maxalign - 1);
	if (a->poison_size > 0) {
		if (a->poison_size > INT64_MAX - addr.u64) {
			/* Insufficient space for poison bytes is not fatal. */
		} else {
			addr.u64 += a->poison_size;
		}
	}
	return addr;
}

static void
arena_ref(struct arena *a)
{
	a->refs++;
}

static void
arena_rele(struct arena *a)
{
	if (--a->refs > 0)
		return;
	free(a);
}

static void
arena_stats_bytes(struct arena *a, size_t bytes)
{
	a->stats.bytes.now += bytes;
	a->stats.bytes.total += bytes;
	if (a->stats.bytes.now > a->stats.bytes.max)
		a->stats.bytes.max = a->stats.bytes.now;
}

static void
arena_stats_frames(struct arena *a, size_t frames)
{
	a->stats.frames.now += frames;
	a->stats.frames.total += frames;
	if (a->stats.frames.now > a->stats.frames.max)
		a->stats.frames.max = a->stats.frames.now;
}

static void *
arena_push(const struct arena *a, struct arena_frame *frame, size_t size)
{
	void *ptr;
	uint64_t newlen;

	if (size > INT64_MAX - frame->len) {
		errno = EOVERFLOW;
		return NULL;
	}
	newlen = frame->len + size;
	if (newlen > frame->size) {
		errno = ENOMEM;
		return NULL;
	}

	frame_unpoison(frame, size);
	ptr = &frame->ptr[frame->len];
	newlen = align_address(a, (union address){.u64 = newlen}).u64;
	/*
	 * Discard alignment if the frame is exhausted, the next allocation will
	 * require a new frame anyway.
	 */
	frame->len = newlen > frame->size ? frame->size : newlen;
	return ptr;
}

static int
arena_frame_alloc(struct arena *a, size_t frame_size)
{
	struct arena_frame *frame;

	frame = malloc(frame_size);
	if (frame == NULL)
		return 0;
	frame->ptr = (char *)frame;
	frame->size = frame_size;
	frame->len = 0;
	frame->next = NULL;
	if (arena_push(a, frame, sizeof(*frame)) == NULL) {
		free(frame);
		return 0;
	}
	frame->next = a->frame;
	a->frame = frame;
	frame_poison(a->frame);

	arena_stats_frames(a, 1);

	return 1;
}

struct arena *
arena_alloc(void)
{
	struct arena *a;
	long page_size;

	page_size = sysconf(_SC_PAGESIZE);
	if (page_size == -1)
		err(1, "sysconf");

	a = calloc(1, sizeof(*a));
	if (a == NULL)
		err(1, "%s", __func__);
	a->frame_size = 16 * (size_t)page_size;
	a->poison_size = POISON_SIZE;
	arena_ref(a);
	if (!arena_frame_alloc(a, a->frame_size))
		err(1, "%s", __func__);

	return a;
}

void
arena_free(struct arena *a)
{
	if (a == NULL)
		return;

	if (a->refs > 1) {
		/* Scope(s) still alive. */
		arena_rele(a);
	} else {
		arena_scope_leave(&(struct arena_scope){.arena = a});
	}
}

void
arena_scope_leave(struct arena_scope *s)
{
	struct arena *a = s->arena;
	struct arena_frame *last_frame = s->frame;
	struct arena_cleanup *ac;
	int idx = a->refs;

	if (idx < MAX_SOURCE_LOCATIONS)
		a->scope_locations[idx] = (struct source_location){0};

	for (ac = s->cleanup; ac != NULL; ac = ac->next)
		ac->fun(ac->ptr);
	s->cleanup = NULL;

	/* Free all frames if the arena is already freed. */
	if (a->refs == 1)
		last_frame = NULL;

	while (a->frame != NULL && a->frame != last_frame) {
		struct arena_frame *frame = a->frame;

		if (frame->size <= a->stats.bytes.now)
			a->stats.bytes.now -= frame->size;
		else
			a->stats.bytes.now = 0;
		if (a->stats.frames.now > 0)
			a->stats.frames.now--;

		a->frame = frame->next;
		free(frame);
	}
	if (a->frame != NULL) {
		a->frame->len = s->frame_len <= a->frame->len ?
		    s->frame_len : 0;
		frame_poison(a->frame);
	}

	a->stats.bytes.now = s->bytes;
	a->stats.frames.now = s->frames;

	arena_rele(a);
}

struct arena_scope
arena_scope_enter_impl(struct arena *a, const char *fun, int lno)
{
	int idx = a->refs;

	if (idx < MAX_SOURCE_LOCATIONS) {
		a->scope_locations[idx] = (struct source_location){
		    .fun = fun,
		    .lno = lno,
		};
	}

	arena_ref(a);
	return (struct arena_scope){
	    .arena	= a,
	    .frame	= a->frame,
	    .frame_len	= a->frame->len,
	    .bytes	= a->stats.bytes.now,
	    .frames	= a->stats.frames.now,
	    .id		= a->refs,
	};
}

static void
arena_scope_validate(struct arena *a, const struct arena_scope *s, size_t size)
{
	const struct source_location fallback = {
		.fun = "?",
		.lno = 0,
	};
	const struct source_location *scope_location;
	int i;

	if (likely(s->id == a->refs))
		return;

	scope_location = s->id - 1 < MAX_SOURCE_LOCATIONS ?
	    &a->scope_locations[s->id - 1] :
	    &fallback;

	fprintf(stderr, "arena: allocating %zu bytes from scope #%d at %s:%d "
	    "which will be freed by nested scope(s):\n",
	    size, s->id, scope_location->fun, scope_location->lno);
	for (i = a->refs; i >= 0 && i > s->id; i--) {
		scope_location = i - 1 < MAX_SOURCE_LOCATIONS ?
		    &a->scope_locations[i - 1] :
		    &fallback;
		fprintf(stderr, "#%d %s:%d\n",
		    i, scope_location->fun, scope_location->lno);
	}

	__builtin_trap();
}

void *
arena_malloc(struct arena_scope *s, size_t size)
{
	struct arena *a = s->arena;
	struct arena_frame *frame;
	void *ptr;
	uint64_t frame_size, total_size;

	arena_scope_validate(a, s, size);

	ptr = arena_push(a, a->frame, size);
	if (ptr != NULL) {
		arena_stats_bytes(a, size);
		return ptr;
	}

	if (sizeof(*frame) > INT64_MAX - size)
		errx(1, "%s: Requested allocation too large", __func__);
	total_size = size + sizeof(*frame);

	frame_size = a->frame_size;
	while (frame_size < total_size) {
		if (frame_size > INT64_MAX / frame_size) {
			errx(1, "%s: Requested allocation exceeds frame size",
			    __func__);
		}
		frame_size <<= 1;
	}

	if (!arena_frame_alloc(a, frame_size))
		err(1, "%s", __func__);

	ptr = arena_push(a, a->frame, size);
	if (ptr == NULL) {
		err(1, "%s", __func__);
		return NULL;
	}
	arena_stats_bytes(a, size);
	return ptr;
}

void *
arena_calloc(struct arena_scope *s, size_t nmemb, size_t size)
{
	void *ptr;
	uint64_t total_size;

	if (nmemb > INT64_MAX / size)
		errx(1, "%s: Requested allocation too large", __func__);
	total_size = nmemb * size;

	ptr = arena_malloc(s, total_size);
	if (ptr == NULL)
		return NULL;
	memset(ptr, 0, total_size);
	return ptr;
}

static int
arena_realloc_fast(struct arena_scope *s, char *ptr, size_t old_size,
    size_t new_size)
{
	struct arena_frame frame;
	struct arena *a = s->arena;
	union address old_addr;

	/* Always allow existing allocations to shrink. */
	if (new_size <= old_size) {
		arena_poison(&ptr[new_size], old_size - new_size);
		return 1;
	}

	/* Check if this is the last allocated object. */
	old_addr.s8 = ptr;
	old_addr.u64 += old_size;
	old_addr = align_address(a, old_addr);
	if (old_addr.s8 != &a->frame->ptr[a->frame->len])
		return 0;

	/* Check if the new size still fits within the current frame. */
	frame = *a->frame;
	frame.len = (size_t)(ptr - frame.ptr);
	if (arena_push(a, &frame, new_size) == NULL)
		return 0;
	arena_stats_bytes(a, new_size - old_size);
	*a->frame = frame;
	return 1;
}

void *
arena_realloc(struct arena_scope *s, void *ptr, size_t old_size,
    size_t new_size)
{
	struct arena *a = s->arena;
	void *new_ptr;
	union address old_addr;

	old_addr.s8 = ptr;
	if ((old_addr.u64 & (maxalign - 1)) != 0) {
		errno = EFAULT;
		return NULL;
	}

	a->stats.realloc.total++;

	/* Fast path while reallocating last allocated object. */
	if (ptr != NULL && arena_realloc_fast(s, ptr, old_size, new_size)) {
		a->stats.realloc.fast++;
		return ptr;
	}

	new_ptr = arena_malloc(s, new_size);
	if (new_ptr == NULL)
		return NULL;
	if (ptr != NULL)
		memcpy(new_ptr, ptr, old_size);
	a->stats.realloc.spill += old_size;
	return new_ptr;
}

char *
arena_sprintf(struct arena_scope *s, const char *fmt, ...)
{
	va_list ap, cp;
	char *str;
	size_t len;
	int n;

	va_start(ap, fmt);

	va_copy(cp, ap);
	n = vsnprintf(NULL, 0, fmt, cp);
	va_end(cp);
	if (n < 0) {
		errno = ENAMETOOLONG;
		err(1, "%s", __func__);
	}

	len = (size_t)n + 1;
	str = arena_malloc(s, len);
	n = vsnprintf(str, len, fmt, ap);
	if (n < 0 || (size_t)n >= len) {
		errno = ENAMETOOLONG;
		err(1, "%s", __func__);
	}

	va_end(ap);
	return str;
}

char *
arena_strdup(struct arena_scope *s, const char *src)
{
	return arena_strndup(s, src, strlen(src));
}

char *
arena_strndup(struct arena_scope *s, const char *src, size_t len)
{
	char *dst;
	uint64_t total_size;

	if (len > INT64_MAX - 1)
		errx(1, "%s: Requested allocation too large", __func__);
	total_size = len + 1;

	dst = arena_malloc(s, total_size);
	if (dst == NULL)
		return NULL;
	memcpy(dst, src, total_size - 1);
	dst[total_size - 1] = '\0';
	return dst;
}

void
arena_cleanup(struct arena_scope *s, void (*fun)(void *), void *ptr)
{
	struct arena_cleanup *ac;

	ac = arena_malloc(s, sizeof(*ac));
	ac->ptr = ptr;
	ac->fun = fun;
	ac->next = s->cleanup;
	s->cleanup = ac;
}

struct arena_stats *
arena_stats(struct arena *a)
{
	return &a->stats;
}

void
arena_poison(void *MAYBE_UNUSED(ptr), size_t MAYBE_UNUSED(size))
{
	ASAN_POISON_MEMORY_REGION(ptr, size);
}
