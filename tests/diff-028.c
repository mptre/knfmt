int
main(void)
{
	nconfigs = 2 * (nlines + 1);
	for (i = 2; i < nintr / nconfigs; i++) {
		/* irq 32 - N */
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh, ICD_ICRn(i * nconfigs), 0);
	}
}
