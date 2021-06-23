int
main(void)
{
	if (nest) {
		if (!aml_evalinteger(lid->abl_softc->sc_acpi,
			lid->abl_softc->sc_devnode, "_LID", 0, NULL, &val) &&
		    val != 0) {
			return;
		}
	}
}
