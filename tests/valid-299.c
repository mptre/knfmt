/*
 * Sqares followed by hard line in brace initializers.
 */

static const struct em_counter em_counters[em_stat_count] = {
	[em_stat_crcerrs] =
	    { "rx crc errs",	KSTAT_KV_U_PACKETS,	E1000_CRCERRS },
	[em_stat_algnerrc] = /* >= em_82543 */
	    { "rx align errs",	KSTAT_KV_U_PACKETS,	0 },
	[em_stat_symerrs] = /* >= em_82543 */
	    { "rx align errs",	KSTAT_KV_U_PACKETS,	0 },
};
