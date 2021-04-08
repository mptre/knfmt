/*
 * Declaration of static queue(3) variable.
 */

static SLIST_HEAD(, timecounter)	tc_list = SLIST_HEAD_INITIALIZER(tc_list);
