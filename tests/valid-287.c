/*
 * Declaration using cpp.
 */

struct pool_page_header {
	XSIMPLEQ_HEAD(, pool_item)
				ph_items;
};
