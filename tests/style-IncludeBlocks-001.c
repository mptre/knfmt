/*
 * IncludeBlocks: Merge
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

#include "test.h"

#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>

#include "libks/buffer.h"

#include "alloc.h"

int
main(void)
{
	return 0;
}
