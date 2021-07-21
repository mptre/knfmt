/*
 * Minimize declaration alignment.
 */

int
main(void)
{
	static uint8_t		 cookie[] = DHCP_COOKIE;
	static struct ether_addr bcast_mac;
	struct ether_header	*eh;
	struct ether_addr	 ether_src, ether_dst;
	struct ip		*ip;
	struct udphdr		*udp;
	struct dhcp_hdr		*dhcp_hdr;
	struct in_addr		 server_identifier, subnet_mask;
	struct in_addr		 nameservers[MAX_RDNS_COUNT];
	struct dhcp_route	 routes[MAX_DHCP_ROUTES];
	size_t			 rem, i;
	uint32_t		 sum, usum, lease_time = 0, renewal_time = 0;
	uint32_t		 rebinding_time = 0;
	uint8_t			*p, dho = DHO_PAD, dho_len;
	uint8_t			 dhcp_message_type = 0;
	int			 routes_len = 0, routers = 0, csr = 0;
	char			 from[sizeof("xx:xx:xx:xx:xx:xx")];
	char			 to[sizeof("xx:xx:xx:xx:xx:xx")];
	char			 hbuf_src[INET_ADDRSTRLEN];
	char			 hbuf_dst[INET_ADDRSTRLEN];
	char			 hbuf[INET_ADDRSTRLEN];
	char			 domainname[4 * 255 + 1];
	char			 hostname[4 * 255 + 1];
	char			 ifnamebuf[IF_NAMESIZE], *if_name;
}
