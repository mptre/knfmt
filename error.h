struct buffer;

struct error	*error_alloc(int);
void		 error_free(struct error *);
void		 error_reset(struct error *);
void		 error_flush(struct error *);
struct buffer	*error_get_buffer(struct error *);

#define error_write(er, fmt, ...) do {					\
	buffer_appendv(error_get_buffer((er)), (fmt), __VA_ARGS__);	\
	error_flush((er));						\
} while (0)

