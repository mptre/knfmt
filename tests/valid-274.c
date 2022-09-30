/*
 * AFL
 */

static void
foo(void)
{
	asm("" : "+r" & (*bar ()));
}
