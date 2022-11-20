#include <stddef.h>	/* size_t */

struct buffer;

void	tracef(unsigned char, const char *, const char *, ...)
	__attribute__((__format__(printf, 3, 4)));

char	*strnice(const char *, size_t);
void	 strnice_buffer(struct buffer *, const char *, size_t);

size_t	strwidth(const char *, size_t, size_t);

int	searchpath(const char *, int *);
