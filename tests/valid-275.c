/*
 * Inline assembler, empty clobbers.
 */

int
main(void)
{
	__asm__ __volatile__("nop" :::);
}
