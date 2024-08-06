struct arena_scope;

struct error	*error_alloc(int, struct arena_scope *);
struct buffer	*error_begin(struct error *);
void		 error_end(struct error *);
void		 error_reset(struct error *);
void		 error_flush(struct error *, int);
