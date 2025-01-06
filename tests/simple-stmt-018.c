/*
 * Statement w/o braces regression.
 */

int
main(void)
{
	if (is_eevex_legacy)
		prefix->selector |= condition_code_to_selector(evex.conditional.scc);
}
