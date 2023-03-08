struct options;

struct files {
	struct file	*fs_vc;			/* VECTOR(struct file) */
};

struct file {
	struct diffchunk	*fe_diff;	/* VECTOR(struct diffchunk) */
	struct error		*fe_error;
	char			*fe_path;
	int			 fe_fd;
};

struct file	*files_alloc(struct files *, const char *,
    const struct options *);
void		 files_free(struct files *);

struct buffer	*file_read(struct file *);
void		 file_close(struct file *);
