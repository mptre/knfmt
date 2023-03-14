/*
 * Brace initializers on a single line.
 */

static void
set_signal_handler(void)
{
	int signals[] = {SIGTERM, SIGHUP, SIGINT, SIGUSR1, SIGUSR2,
	    SIGPIPE, SIGXCPU, SIGXFSZ, 0};
}
