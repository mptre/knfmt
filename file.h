struct config;

struct files {
	struct file	*fs_vc;			/* VECTOR(struct file) */
};

struct file {
	struct diffchunk	*fe_diff;	/* VECTOR(struct diffchunk) */
	struct error		*fe_error;
	char			*fe_path;
};

struct file	*files_alloc(struct files *, const char *,
    const struct config *);
void		 files_free(struct files *);
