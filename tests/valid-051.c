/*
 * Parenthesis inside brace initializer.
 */

struct foo	f[] = { KEY(ALT_ENTER, "\033\n") };

struct foo	f[] = {
	KEY(ALT_ENTER, "\033\n"),
	KEY(ALT_ENTER, "\033\n"),
};
