#include "options.h"
#include "style.h"

int
main(void)
{
	struct options op;
	struct style *st;

	options_init(&op);
	style_init();
	st = style_parse("/dev/stdin", &op);
	style_free(st);
	style_teardown();
	return 0;
}
