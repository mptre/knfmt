/*
 * IncludeBlocks: Regroup
 * IncludeCategories:
 *   - Regex: '^<'
 *     Priority: 1
 *   - Regex: '^"'
 *     Priority: 2
 * SortIncludes: CaseSensitive
 */

#include "b.h"

#define NEIN

#include "a.h"

int nein;

#include "b.h"

extern "NEIN" {
}

#include "a.h"
