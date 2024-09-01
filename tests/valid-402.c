/*
 * Inline assembler, regression no inputs/outputs/clobbers/labels.
 */

int
main(void)
{
	__asm volatile("sti");

	switch (0) {
	default:
	}
}
