/*
 * Align struct declarations.
 */

struct s {
	int		 s_x;
	const char	*s_ptr;

	int		 s_y;

	TAILQ_ENTRY(s)	 s_entry;

	int		 s_z;
	TAILQ_ENTRY(s)	 s_entry;
};
