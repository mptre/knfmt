#include <limits.h>	/* ULONG_MAX */
#include <stddef.h>	/* size_t */

#define VECTOR(type) type *

#define VECTOR_INIT(vc) __extension__ ({				\
	size_t _i = vector_init((void **)&(vc), sizeof(*(vc)));		\
	_i == ULONG_MAX ? NULL : (vc) + _i;				\
})
size_t	vector_init(void **, size_t);

#define VECTOR_FREE(vc) vector_free((void **)&(vc))
void	vector_free(void **);

#define VECTOR_RESERVE(vc, n) __extension__ ({				\
	size_t _i = vector_reserve((void **)&(vc), n);			\
	_i == ULONG_MAX ? NULL : (vc) + _i;				\
})
size_t	vector_reserve(void **, size_t);

#define VECTOR_ALLOC(vc) __extension__ ({				\
	size_t _i = vector_alloc((void **)&(vc), 0);			\
	_i == ULONG_MAX ? NULL : (vc) + _i;				\
})
#define VECTOR_CALLOC(vc) __extension__ ({				\
	size_t _i = vector_alloc((void **)&(vc), 1);			\
	_i == ULONG_MAX ? NULL : (vc) + _i;				\
})
size_t	vector_alloc(void **, int);

#define VECTOR_POP(vc) __extension__ ({					\
	size_t _i = vector_pop((void *)(vc));				\
	_i == ULONG_MAX ? NULL : (vc) + _i;				\
})
size_t	vector_pop(void *);

#define VECTOR_FIRST(vc) __extension__ ({				\
	size_t _i = vector_first((void *)(vc));				\
	_i == ULONG_MAX ? NULL : (vc) + _i;				\
})
size_t	vector_first(void *);

#define VECTOR_LAST(vc) __extension__ ({				\
	size_t _i = vector_last((void *)(vc));				\
	_i == ULONG_MAX ? NULL : (vc) + _i;				\
})
size_t	vector_last(void *);

#define VECTOR_LENGTH(vc) vector_length((void *)(vc))
size_t	vector_length(void *);

#define VECTOR_EMPTY(vc) (VECTOR_LENGTH(vc) == 0)
