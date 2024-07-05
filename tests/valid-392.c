/*
 * Declaration with attribute before initialization.
 */

static void (*x)(void *) __attribute__((__used__)) =
    func;
