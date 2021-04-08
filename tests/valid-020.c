/*
 * Preprocessor directive within if statement.
 */

int
main(void)
{
	if (sc->sc_base.me_evp == NULL
#if NWSDISPLAY > 0
	    && sc->sc_displaydv == NULL
#endif
	    )
		return;
}
