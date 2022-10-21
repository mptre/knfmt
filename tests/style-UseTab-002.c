/*
 * UseTab: Never
 */

#define RESOLVE_ITEM_STR(field)                                         \
    static const char * resolve_ ## field (void *item, void *opaque)    \
    {                                                                   \
        return ((mrss_item_t *)item)->field;                            \
    }                                                                   \

RESOLVE_ITEM_STR(title);
