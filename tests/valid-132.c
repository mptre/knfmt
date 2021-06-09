/*
 * Declaration using cpp.
 */

struct event	proc_rd, proc_wr;
union {
	struct {
		SIMPLEQ_HEAD(, ident_resolver)	replies;
	} parent;
	struct {
		SIMPLEQ_HEAD(, ident_client)	pushing, popping;
	} child;
} sc;
