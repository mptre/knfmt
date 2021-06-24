/*
 * Optional line before binary operator must be discarded.
 */

int
main(void)
{
	if (!(vmm_softc->mode == VMM_MODE_EPT
	    || vmm_softc->mode == VMM_MODE_RVI))
		return 0;
}
