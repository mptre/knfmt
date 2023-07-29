/*
 * Sense cpp macro alignment.
 */

int
placeholders_setup(struct ofmt *ofmt)
{
    #define PLACEHOLDER(key, field, ...) \
        if (ofmt_set_resolver(ofmt, key, resolve_ ## field) == -1) \
            goto fail;
    #include "placeholders_gen.h"
    #undef PLACEHOLDER
}
