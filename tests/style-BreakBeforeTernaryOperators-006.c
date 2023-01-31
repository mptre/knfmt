/*
 * BreakBeforeTernaryOperators: true
 */

int
main(void)
{
	aaaaaa bbbbbbbbbbbbbbbb = (cccccc % 4096) ? cccccc + 4096 - (cccccc % 4096)
						: cccccc;  // XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
}
