/*
 * Long expressions causing optline(s) to be disabled.
 */

int
main(void)
{

	if ((aaaaaaaaaaa(pr, dc,
	     CCCCCCCCCCCCCCCCC | CCCCCCCCCCCCCCCCCC | CCCCCCCCCCCCCCCCCCCCC) & XXXX) ||
	    (bbbbbbbbbbbbbbbb(pr, dc) & XXXX) ||
	    (ccccccccccccccccc(pr, dc) & XXXX))
		return 1;
	return 0;
}
