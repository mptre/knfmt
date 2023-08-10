/*
 * Sort includes.
 */

#include "a.h"
#include <b.h>

#include <a.h>
#include <b/b.h>

#include <b/b.h>
#include <a/a.h>

#include <b.h>
#include <a.h>

#include "b.h"
#include "a.h"

#include "b.h"
#include "a.h"
#define FOO

#include "b.h"
#include "a.h"
extern int foo;

#include "b.h"
#include "a.h"

extern int foo;

#include "b.h"
#include "a.h"

int
main(void)
{
	return 0;
}
