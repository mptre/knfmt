/*
 * Brace initializers with cpp macros.
 */

static struct key keys[] = {
	KEYMAP(ALT_ENTER,	"\033\n"),
	TIO(CTRL_C,		VINTR),
	CAP(DEL,		"kdch1"),

	KEY(UNKNOWN,	NULL),
};
