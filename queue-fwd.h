#define TAILQ_HEAD(name, type)						\
struct name {								\
	struct type *tqh_first;						\
	struct type **tqh_last;						\
}

#define TAILQ_ENTRY(type)						\
struct {								\
	struct type *tqe_next;						\
	struct type **tqe_prev;						\
}
