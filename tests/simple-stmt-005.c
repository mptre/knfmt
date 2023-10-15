/*
 * Missing curly braces around single statement spanning multiple lines.
 */

int
main(void)
{
	if (0) {
	} else {
		if (1)
			indent = parser_simple_stmt_ifelse_enter(pr, indent,
			    &simple);
	}
}
