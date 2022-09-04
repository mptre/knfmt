#include "cdefs.h"

struct options {
	unsigned int	op_flags;
#define OPTIONS_FLAG_DIFF		0x00000001u
#define OPTIONS_FLAG_DIFFPARSE		0x00000002u
#define OPTIONS_FLAG_INPLACE		0x00000004u
#define OPTIONS_FLAG_SIMPLE		0x00000008u
#define OPTIONS_FLAG_TEST		0x80000000u

	unsigned int	op_verbose;
	unsigned int	op_mw;		/* max width per line */
	unsigned int	op_tw;		/* tab width */
	unsigned int	op_sw;		/* soft width */
};

void	options_init(struct options *);

static inline int
options_trace(const struct options *op)
{
	return UNLIKELY(op->op_verbose >= 2);
}
