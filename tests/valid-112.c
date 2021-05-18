/*
 * Switch cpp branch.
 */

int
sysctl_tty(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	int err;

	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case KERN_TTY_TKNIN:
		return (sysctl_rdquad(oldp, oldlenp, newp, tk_nin));
	case KERN_TTY_TKNOUT:
		return (sysctl_rdquad(oldp, oldlenp, newp, tk_nout));
	case KERN_TTY_TKRAWCC:
		return (sysctl_rdquad(oldp, oldlenp, newp, tk_rawcc));
	case KERN_TTY_TKCANCC:
		return (sysctl_rdquad(oldp, oldlenp, newp, tk_cancc));
	case KERN_TTY_INFO: {
		struct itty *ttystats;
		size_t ttystatssiz;
		int ttyc;

		ttystats_init(&ttystats, &ttyc, &ttystatssiz);
		err = sysctl_rdstruct(oldp, oldlenp, newp, ttystats,
		    ttyc * sizeof(struct itty));
		free(ttystats, M_SYSCTL, ttystatssiz);
		return (err);
	}
	default:
#if NPTY > 0
		return (sysctl_pty(name, namelen, oldp, oldlenp, newp, newlen));
#else
		return (EOPNOTSUPP);
#endif
	}
	/* NOTREACHED */
}
