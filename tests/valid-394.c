/*
 * Inline assembler.
 */

int
main(void)
{
	asm("cpuid" : "+c" (c), "+d" (d) : "a" (1), "c" (0) : "ebx");
}
