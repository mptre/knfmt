/*
 * Function argument cpp branch.
 */

static void
log_addr(const char *descr,
#ifdef INET6
    struct sockaddr_storage *addr,
#else
    struct sockaddr_in *addr,
#endif
    short family)
{
}
