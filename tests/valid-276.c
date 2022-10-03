/*
 * Inline assembler, trim right parenthesis.
 */

int
main(void)
{
	__asm__ __volatile__(
	    "mov %1, %%rax;\n"
	    "mov %0, %%rbx;\n"
	    "crc32 %%rax, %%rbx;\n"
	    "mov %%rbx, %0;\n"
	    : "=m" (b)
	    : "m" (a)
	    :
	);
}
