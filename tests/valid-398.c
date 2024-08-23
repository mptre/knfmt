/*
 * Binary expression erroneously interpreted as a cast.
 */

int
main(void)
{
	(int)(sa) +
	    len;
}
