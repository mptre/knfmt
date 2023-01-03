/*
 * Inline assembler, top level.
 */

void
efi_fault(void)
{
	longjmp(&efi_jmpbuf);
}
__asm(".pushsection .nofault, \"a\"; .quad efi_fault; .popsection");
