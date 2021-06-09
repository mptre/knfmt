/*
 * Declaration cpp branch, lexer_recover_purge() regression.
 */

#ifndef SMALL_KERNEL

int
acpi_filtread(struct knote *kn, long hint)
{
	/* XXX weird kqueue_scan() semantics */
	if (hint && !kn->kn_data)
		kn->kn_data = hint;
	return (1);
}

int
acpikqfilter(dev_t dev, struct knote *kn)
{
}

#else /* SMALL_KERNEL */

int
acpikqfilter(dev_t dev, struct knote *kn)
{
	return (EOPNOTSUPP);
}

#endif /* SMALL_KERNEL */
