/*
 * Inline assembler in declaration.
 */

int
main(void)
{
	register long *x0 __asm__("x0") = sp;
}
