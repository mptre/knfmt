/*
 * Long expression with trailing comment.
 */

int
main(void)
{
	if (CC(e->index == MSR_GS_BASE) ||
	    CC(e->index == MSR_IA32_SMM_MONITOR_CTL) || /* SMM is not supported */
	    nested_vmx_msr_check_common(vcpu, e))
		return -EINVAL;
	return 0;
}
