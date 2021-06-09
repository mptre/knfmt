/*
 * Function argument cpp branch.
 */

void
addr2str(
#ifdef INET6
    struct sockaddr_storage *addr
#else
    struct sockaddr_in *addr
#endif
    , char *str, size_t len)
{
#ifdef INET6
	if (addr->ss_family == AF_INET6) {
		if (!inet_ntop(AF_INET6,
			&((struct sockaddr_in6 *)addr)->sin6_addr, str, len))
			strlcpy(str, "[unknown ip6, inet_ntop failed]", len);
		return;
	}
#endif
	if (!inet_ntop(AF_INET, &((struct sockaddr_in *)addr)->sin_addr, str,
		    len))
		strlcpy(str, "[unknown ip4, inet_ntop failed]", len);
}
