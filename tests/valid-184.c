/*
 * Brace initializers with cpp branch.
 */

#if NWSMUX > 0 || NWSDISPLAY > 0
struct wssrcops wskbd_srcops = {
	.type		= WSMUX_KBD,
	.dopen		= wskbd_mux_open,
	.dclose		= wskbd_mux_close,
	.dioctl		= wskbd_do_ioctl,
	.ddispioctl	= wskbd_displayioctl,
#if NWSDISPLAY > 0
	.dsetdisplay	= wskbd_set_display,
#else
	.dsetdisplay	= NULL,
#endif
};
#endif
