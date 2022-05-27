/*
 * Single space before goto labels are preserved.
 */

int
main(void)
{
 out0:
	return 0;

  out1:
	return 1;

	/* Comment before label. */
 out2:
	return 2;

	out3:
	return 3;
}
