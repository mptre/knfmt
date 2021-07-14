int
main(void)
{
	if (tdb->tdb_udpencap_port)
		i += sizeof(struct sadb_x_udpencap);

	if (tdb->tdb_mtu > 0)
		i+= sizeof(struct sadb_x_mtu);

	if (tdb->tdb_rdomain != tdb->tdb_rdomain_post)
		i += sizeof(struct sadb_x_rdomain);
}
