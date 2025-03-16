#include "config.h"

#include "libks/arena.h"
#include "libks/compiler.h"
#include "libks/fuzzer.h"

#include "options.h"
#include "style.h"

struct test_context {
	struct options	 op;
	struct arena	*eternal;
	struct arena	*scratch;
};

static void *
init(int UNUSED(argc), char **UNUSED(argv))
{
	static struct test_context c;

	style_init();

	options_init(&c.op);
	c.eternal = arena_alloc("eternal");
	c.scratch = arena_alloc("scratch");
	return &c;
}
FUZZER_INIT(init);

static void
teardown(void *userdata)
{
	struct test_context *c = userdata;

	arena_free(c->scratch);
	arena_free(c->eternal);

	style_shutdown();
}
FUZZER_TEARDOWN(teardown);

static void
target(const struct buffer *bf, void *userdata)
{
	struct test_context *c = userdata;

	arena_scope(c->eternal, s);

	style_parse_buffer(bf, ".clang-format", &s, c->scratch, &c->op);
}
FUZZER_TARGET_BUFFER(target);
