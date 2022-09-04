/*
 * Inline assembler.
 */

static void
foo(void)
{
	asm("" : "+r" (*bar()));
}
