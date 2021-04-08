/*
 * Hard line among initializers must disable alignment.
 */

struct cfattach	acpibtn_ca = {
	sizeof(struct acpibtn_softc), acpibtn_match, acpibtn_attach, NULL,
	acpibtn_activate
};
