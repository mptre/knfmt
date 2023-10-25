/*
 * IncludeBlocks: Regroup
 * IncludeCategories:
 *   - Regex: '^"config\.h"'
 *     Priority: 1
 *   - Regex: '^<sys/types\.h'
 *     Priority: 2
 *     SortPriority: 0
 *   - Regex: '^<sys/'
 *     Priority: 2
 *   - Regex: '^<'
 *     Priority: 3
 *   - Regex: '^"libks/'
 *     Priority: 4
 *   - Regex: '^"'
 *     Priority: 5
 * SortIncludes: CaseSensitive
 */

#include "alloc.h"
#include "config.h"
#include "diff.h"
#include "error.h"
#include "libks/buffer.h"
#include "libks/compiler.h"
#include "libks/vector.h"
#include "options.h"
#include "test.h"
#include "token.h"
#include "util.h"
#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

int
main(void)
{
	return 0;
}
