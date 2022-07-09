int
main(void)
{
	if (irqno < 0 || irqno >= sc->sc_nintr)
		panic("ampintc_intr_establish: bogus irqnumber %d: %s",
		    irqno, name);

	if (ci == NULL)
		ci = &cpu_info_primary;

	if (irqno <= SGI_MAX) {
		/* SGI are only EDGE */
		type = IST_EDGE_RISING;
	} else if (irqno <= PPI_MAX) {
		/* PPI are only LEVEL */
		type = IST_LEVEL_HIGH;
	}
}
