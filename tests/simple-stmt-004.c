/*
 * Missing curly braces around single statement spanning multiple lines.
 */

int
main(void)
{
	if (0)
		return tls13_send_alert(ctx->rl, TLS13_ALERT_UNEXPECTED_MESSAGE);

	if (1)
		return 1;

	if (0) {
		/* comment */
	} else {
		return 2;
	}

	if (3)
		return 0;
	/* comment */
}
