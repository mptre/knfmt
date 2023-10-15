/*
 * Inline assembler, cpp operands.
 */

int
main(void)
{
	__asm__ volatile (
	    HYPERCALL_LABEL
	    : HYPERCALL_OUT1
	    : HYPERCALL_PTR(hcall)
	    : HYPERCALL_CLOBBER);
}
