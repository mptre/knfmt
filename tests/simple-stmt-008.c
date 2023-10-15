/*
 * No braces wanted around statements with cpp branches.
 */

int
main(void)
{
	if (1)
#if 1
		return 1;
#else
		return 0;
#endif
}
