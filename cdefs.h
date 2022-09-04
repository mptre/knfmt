#define UNUSED(x)	_##x __attribute__((__unused__))
#define MAYBE_UNUSED(x)	x __attribute__((__unused__))

#define UNLIKELY(x)	__builtin_expect((x), 0)
