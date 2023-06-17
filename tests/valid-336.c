/*
 * Inline assembler, goto labels.
 */

int
main(void)
{
	asm inline goto (
	    "btl %1, %0\n\t"
	    "jc %l2"
	    : /* No outputs. */
	    : "r" (p1), "r" (p2)
	    : "cc"
	    : carry);
}
