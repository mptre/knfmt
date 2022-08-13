/*
 * Wrong indent after entering the else branch and going unmute.
 */

int
dk_mountroot(void)
{
	switch (1) {
	default:
#ifdef FFS
	{
		extern int ffs_mountroot(void);

		printf("filesystem type %d not known.. assuming ffs\n",
		    dl.d_partitions[part].p_fstype);
		mountrootfn = ffs_mountroot;
	}
#else
		panic("disk 0x%x filesystem type %d not known",
		    rootdev, dl.d_partitions[part].p_fstype);
#endif
	}
	return (*mountrootfn)();
}
