/*
 * Optional line regression.
 */

int
main(void)
{
	vcpu->vc_msr_bitmap_va = (vaddr_t)km_alloc(PAGE_SIZE, &kv_page, &kp_zero,
	    &kd_waitok);
}
