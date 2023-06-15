/*
 * Brace initializers, soft line expression regression.
 */

static const struct grammar robsd_regress[] = {
	{ "regress",		LIST,		config_parse_regress,		REQ|REP,	{ NULL } },
	{ "regress-env",	LIST,		config_parse_regress_env,	REP,		{ NULL } },
};
