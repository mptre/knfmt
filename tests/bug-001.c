/*
 * X macro confusion.
 */

/* $OpenBSD: m_item_nam.c,v 1.7 2010/01/12 23:22:08 nicm Exp $ */

#include "menu.priv.h"

MODULE_ID("$Id: m_item_nam.c,v 1.7 2010/01/12 23:22:08 nicm Exp $")

NCURSES_EXPORT(const char *)
item_name(const ITEM * item)
{
  T((T_CALLED("item_name(%p)"), item));
  returnCPtr((item) ? item->name.str : (char *)0);
}

NCURSES_EXPORT(const char *)
item_description(const ITEM * item)
{
  T((T_CALLED("item_description(%p)"), item));
  returnCPtr((item) ? item->description.str : (char *)0);
}

/* m_item_nam.c ends here */
