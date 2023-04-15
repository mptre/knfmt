/*
 * Missing curly braces around single statement spanning multiple lines.
 */

int
main(void)
{
	if (0) {
		if (1) {
			if (aaaaaaaaaaaaaa(slot))
				/* comment */ after = bbbbbbbbbbbbbbbbbbbbbbbbbbbb(sd, dt, ds, after);
			else
				after = ccccccccccccccccccccc(sd, dt, ds, after);
			/* comment */
		}
	}
}
