#include <string.h>

#include "extern.h"

void
config_init(struct config *cf)
{
	memset(cf, 0, sizeof(*cf));
	cf->cf_mw = 80;
	cf->cf_tw = 8;
	cf->cf_sw = 4;
}
