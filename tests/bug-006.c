/*
 * Wrong indent after entering the else branch and going unmute. Caused by the
 * last emitted indent by the expr above.
 */

int
main(void)
{

	if (0) {
		if (1) {
#if 0
			x = 0 &
			    1;
#else
			y = 2 &
			    3;
#endif
		}
	}
}
