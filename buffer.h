#include <stdarg.h>	/* va_list */
#include <stddef.h>	/* size_t */

struct buffer {
	char	*bf_ptr;
	size_t	 bf_siz;
	size_t	 bf_len;
};

struct buffer	*buffer_alloc(size_t);
struct buffer	*buffer_read(const char *);
struct buffer	*buffer_read_fd(int);
void		 buffer_free(struct buffer *);
void		 buffer_puts(struct buffer *, const char *, size_t);
void		 buffer_putc(struct buffer *, char);
void		 buffer_printf(struct buffer *, const char *, ...)
	__attribute__((__format__(printf, 2, 3)));
void		 buffer_vprintf(struct buffer *, const char *, va_list);
size_t		 buffer_indent(struct buffer *, int, int, size_t);
char		*buffer_release(struct buffer *);
void		 buffer_reset(struct buffer *);
int		 buffer_cmp(const struct buffer *, const struct buffer *);
