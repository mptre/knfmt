/*
 * BreakBeforeBinaryOperators: All
 */

#if defined(FOO)

int
main(void)
{
	return 1111111111111111111111111111111111111 || 222222222222222222222222222222;
}

#else /* !defined(FOO) */

int
main(void)
{
	return 0;
}

#endif /* defined(FOO) */
