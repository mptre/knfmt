/*
 * Preprocessor with intertwined comment.
 */

#define SLIST_HEAD(name, type)						\
struct name {								\
	struct type *slh_first;	/* first element */			\
}
