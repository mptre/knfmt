/*
 * Usage of cdefs.h macros regression.
 */

__BEGIN_HIDDEN_DECLS

#if defined(__GNUC__)
__attribute((aligned(4096)))
#elif defined(_MSC_VER)
__declspec(align(4096))
#elif defined(__SUNPRO_C)
# pragma align 4096(ecp_nistz256_precomputed)
#endif

int
main(void)
{
}
