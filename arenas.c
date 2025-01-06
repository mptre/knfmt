#include "arenas.h"

#include "config.h"

#include "libks/arena.h"

void
arenas_init(struct arenas *a)
{
	a->eternal = arena_alloc();
	a->scratch = arena_alloc();
	a->doc = arena_alloc();
	a->buffer = arena_alloc();
	a->ruler = arena_alloc();
}

void
arenas_free(struct arenas *a)
{
	arena_free(a->ruler);
	arena_free(a->buffer);
	arena_free(a->doc);
	arena_free(a->scratch);
	arena_free(a->eternal);
}
