#include <stddef.h>	/* size_t */

struct arena_scope;
struct buffer;

const char	*path_slice(const char *, unsigned int, struct arena_scope *);
int		 is_path_header(const char *);

unsigned int	colwidth(const char *, size_t, unsigned int);
size_t		strwidth(const char *, size_t, size_t);

const char	*strnice(const char *, size_t, struct arena_scope *);

size_t	strindent_buffer(struct buffer *, size_t, int, size_t);
