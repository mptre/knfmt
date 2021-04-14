/*
 * Squares in brace initializers.
 */

static const struct vcpu_reg_state	vcpu_init_flat64 = {
	.vrs_gprs[VCPU_REGS_RFLAGS]	= 0x2,
	.vrs_gprs[VCPU_REGS_RIP]	= 0x0,
};
