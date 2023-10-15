/*
 * Array pointer cast.
 */

int
main(void)
{
	slaves[i].tblock = (char (*)[TP_BSIZE])
	    (((long)&buf[ntrec + 1] + pgoff) & ~pgoff);
}
