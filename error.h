struct arena_scope;

struct error	*error_alloc(struct arena_scope *, int);
struct buffer	*error_begin(struct error *);
void		 error_end(struct error *);
void		 error_reset(struct error *);
void		 error_flush(struct error *, int);
