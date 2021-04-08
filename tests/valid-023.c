/*
 * Comment within if statement.
 */

int
main(void)
{
	if (sc->sc_base.me_evp == NULL
	/* comment */
	    && sc->sc_displaydv == NULL)
		return;
}
