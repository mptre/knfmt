/*
 * Broken code hidden behind cpp.
 */

int
cztty_receive(struct cztty_softc *sc, struct tty *tp)
{
	struct cz_softc *cz = CZTTY_CZ(sc);
	u_int get, put, size, address;
	int done = 0, ch;

	size = CZTTY_BUF_READ(sc, BUFCTL_RX_BUFSIZE);
	get = CZTTY_BUF_READ(sc, BUFCTL_RX_GET);
	put = CZTTY_BUF_READ(sc, BUFCTL_RX_PUT);
	address = CZTTY_BUF_READ(sc, BUFCTL_RX_BUFADDR);

	while ((get != put) &&
	    ((tp->t_canq.c_cc + tp->t_rawq.c_cc) < tp->t_hiwat)) {
#ifdef HOSTRAMCODE
		if (hostram)
			ch = ((char *)fifoaddr)[get];
		} else {
#endif
		ch = bus_space_read_1(cz->cz_win_st, cz->cz_win_sh,
		    address + get);
#ifdef HOSTRAMCODE
		}
#endif
		(*linesw[tp->t_line].l_rint)(ch, tp);
		get = (get + 1) % size;
		done = 1;
	}
	if (done) {
		CZTTY_BUF_WRITE(sc, BUFCTL_RX_GET, get);
	}
	return (done);
}
