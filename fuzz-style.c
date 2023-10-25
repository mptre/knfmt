#include "config.h"

#include "libks/arena.h"

#include "options.h"
#include "style.h"

int
main(void)
{
	struct arena *eternal, *scratch;
	struct options op;
	struct style *st;

	options_init(&op);
	style_init();
	eternal = arena_alloc(ARENA_FATAL);
	scratch = arena_alloc(ARENA_FATAL);
	arena_scope(eternal, eternal_scope);

	st = style_parse("/dev/stdin", &eternal_scope, scratch, &op);
	style_free(st);

	style_shutdown();
	arena_free(scratch);
	arena_free(eternal);
	return 0;
}
