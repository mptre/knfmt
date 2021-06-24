/*
 * Switch case statements on same line.
 */

int
main(void)
{
	switch (dir) {
	case CR_WRITE:
		if (crnum == 0 || crnum == 4) {
			switch (reg) {
			case 0: r = vcpu->vc_gueststate.vg_rax; break;
			case 1: r = vcpu->vc_gueststate.vg_rcx; break;
			case 2: r = vcpu->vc_gueststate.vg_rdx; break;
			case 3: r = vcpu->vc_gueststate.vg_rbx; break;
			case 4: if (vmread(VMCS_GUEST_IA32_RSP, &r)) {
					printf("%s: unable to read guest "
					    "RSP\n", __func__);
					return (EINVAL);
				}
				break;
			case 5: r = vcpu->vc_gueststate.vg_rbp; break;
			case 6: r = vcpu->vc_gueststate.vg_rsi; break;
			case 7: r = vcpu->vc_gueststate.vg_rdi; break;
			case 8: r = vcpu->vc_gueststate.vg_r8; break;
			case 9: r = vcpu->vc_gueststate.vg_r9; break;
			case 10: r = vcpu->vc_gueststate.vg_r10; break;
			case 11: r = vcpu->vc_gueststate.vg_r11; break;
			case 12: r = vcpu->vc_gueststate.vg_r12; break;
			case 13: r = vcpu->vc_gueststate.vg_r13; break;
			case 14: r = vcpu->vc_gueststate.vg_r14; break;
			case 15: r = vcpu->vc_gueststate.vg_r15; break;
			}
			DPRINTF("%s: mov to cr%d @ %llx, data=0x%llx\n",
			    __func__, crnum, vcpu->vc_gueststate.vg_rip, r);
		}

		if (crnum == 0)
			vmx_handle_cr0_write(vcpu, r);

		if (crnum == 4)
			vmx_handle_cr4_write(vcpu, r);

		break;
	case CR_READ:
		DPRINTF("%s: mov from cr%d @ %llx\n", __func__, crnum,
		    vcpu->vc_gueststate.vg_rip);
		break;
	case CR_CLTS:
		DPRINTF("%s: clts instruction @ %llx\n", __func__,
		    vcpu->vc_gueststate.vg_rip);
		break;
	case CR_LMSW:
		DPRINTF("%s: lmsw instruction @ %llx\n", __func__,
		    vcpu->vc_gueststate.vg_rip);
		break;
	default:
		DPRINTF("%s: unknown cr access @ %llx\n", __func__,
		    vcpu->vc_gueststate.vg_rip);
	}
}
