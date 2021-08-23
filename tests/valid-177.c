/*
 * Any cpp branch inside a struct declaration does not terminate the current
 * alignment scope.
 */

struct s {
#if 1
	const char	*s;
#endif

	int		 i;
};
