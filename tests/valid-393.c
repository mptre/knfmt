/*
 * Redundant semicolons after statements.
 */

int
main(void)
{
	goto foo;;
	continue;;
	return;;
	break;;
	do {
	} while (0);;

	{
		int x;
	};
}

int
main(void)
{
#if 0
	}
#endif
;
}