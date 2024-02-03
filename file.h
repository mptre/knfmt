struct buffer;

struct files {
	struct file	*fs_vc;			/* VECTOR(struct file) */
};

struct file {
	struct diffchunk	*fe_diff;	/* VECTOR(struct diffchunk) */
	char			*fe_path;
	int			 fe_fd;
};

struct file	*files_alloc(struct files *, const char *);
void		 files_free(struct files *);

int	file_read(struct file *, struct buffer *);
void	file_close(struct file *);
