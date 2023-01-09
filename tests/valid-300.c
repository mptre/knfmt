/*
 * Inline assembler, symbolic names.
 */

int
main(int argc, char *argv[])
{
	asm("mov %[x], %%eax" :: [x] "r" (argc));
}
