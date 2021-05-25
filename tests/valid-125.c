/*
 * Statement cpp branch.
 */

int
main(void)
{
#if defined(__alpha__)
	if (direction == AUMODE_RECORD)
		ed->ed_dmat = alphabus_dma_get_tag(sc->sc_dmat, ALPHA_BUS_ISA);
	else
#elif defined(__amd64__) || defined(__i386__)
	if (direction == AUMODE_RECORD)
		ed->ed_dmat = &isa_bus_dma_tag;
	else
#endif
		ed->ed_dmat = sc->sc_dmat;
}
