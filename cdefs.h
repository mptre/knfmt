#define UNUSED(x)	_##x __attribute__((__unused__))
#define MAYBE_UNUSED(x)	x __attribute__((__unused__))

#if __has_attribute(__fallthrough__)
#define FALLTHROUGH __attribute__((__fallthrough__))
#else
#define FALLTHROUGH do {} while (0)  /* FALLTHROUGH */
#endif

#define UNLIKELY(x)	__builtin_expect((x), 0)
