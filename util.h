#include <stddef.h>	/* size_t */

struct arena_scope;
struct buffer;

unsigned int	colwidth(const char *, size_t, unsigned int, unsigned int *);

const char	*path_slice(const char *, unsigned int, struct arena_scope *);

size_t	strindent_buffer(struct buffer *, size_t, int, size_t);

const char	*strnice(const char *, size_t, struct arena_scope *);
void		 strnice_buffer(struct buffer *, const char *, size_t);

size_t	strwidth(const char *, size_t, size_t);
