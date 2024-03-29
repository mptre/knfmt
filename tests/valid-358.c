/*
 * Sense cpp macro alignment.
 */

#define RESOLVE_ITEM_STR(field) \
    static const char * resolve_ ## field (void *item, void *opaque)    \
    {                                                                   \
        return ((mrss_item_t *)item)->field;                            \
    }                                                                   \

#define RESOLVE_ITEM_INT(field) \
    static const char * resolve_ ## field (void *item, void *opaque)    \
    {                                                                   \
        struct state *state = opaque;                                   \
        int n;                                                          \
                                                                        \
        n = snprintf(state->bytes, state->buflen, "%d",                 \
                     ((mrss_item_t *)item)->field);                     \
                                                                        \
        if (n >= state->buflen)                                         \
             panic("not enough buffer");                                \
                                                                        \
        return state->bytes;                                            \
    }                                                                   \
