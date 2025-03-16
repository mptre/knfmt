#include "arenas.h"

#include "config.h"

#include "libks/arena.h"

void
arenas_init(struct arenas *a)
{
	a->eternal = arena_alloc("eternal");
	a->scratch = arena_alloc("scratch");
	a->doc = arena_alloc("doc");
	a->buffer = arena_alloc("buffer");
	a->ruler = arena_alloc("ruler");
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
