/*
 * Wrong alignment in brace initializers.
 */

STATIC const struct usbd_bus_methods dwc2_bus_methods = {
	.open_pipe =	dwc2_open,
	.dev_setaddr =	dwc2_setaddr,
	.soft_intr =	dwc2_softintr,
	.do_poll =	dwc2_poll,
	.allocx =	dwc2_allocx,
	.freex =	dwc2_freex,
};
