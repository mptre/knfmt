/*
 * Brace initializers with cpp branch.
 */

struct emul	emul_elf = {
	sysent,
#ifdef SYSCALL_DEBUG
	syscallnames,
#else
	NULL,
#endif
	sigcoderet
};
