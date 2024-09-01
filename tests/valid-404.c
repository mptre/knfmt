/*
 * Declaration empty followed by long expression in for loop.
 */

int
main(void)
{
	for (; n > 0; n=m, m=0, np = get_addressed_line_node(current_addr + 1))
		;
}
