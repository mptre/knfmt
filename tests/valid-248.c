/*
 * Inline assembler.
 */

int
main(void)
{
	__asm("swap.b %1, %0" : "=r" (y) : "r" (x));
	__asm("swap.w %1, %0" : "=r" (y) : "r" (y));
	__asm volatile ("swap.b %1, %0" : "=r" (y) : "r" (y));
	__asm __volatile ("swap.b %1, %0" : "=r" (y) : "r" (y));

	__asm volatile (
	"	.set	push\n"
	"	.set	mips64r2\n"
	"	rdhwr	%0, $2\n"
	"	.set	pop\n"
	: "=r" (count));

	__asm__("1: ldaxr %w0, [%x1]       \n"
		"   stlxr %w2, %w3, [%x1]  \n"
		"   cmp %w2, #0            \n"
		"   bne 1b                 \n"
		: "+r" (old), "+r" (lock), "+r" (scratch)
		: "r" (_ATOMIC_LOCK_LOCKED));

	__asm __volatile("mrs %x0, fpcr" : "=&r" (old));

	__asm volatile("isb" ::: "r0", "memory");

	__asm volatile("mov x18, %0\n"
	    "msr tpidr_el1, %0" :: "r"(&cpu_info_primary));

	__asm volatile("mrs %x0, "STR(ICC_CTLR_EL1) : "=r"(ctrl));
}
