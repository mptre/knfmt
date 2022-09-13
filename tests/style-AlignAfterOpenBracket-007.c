/*
 * AlignAfterOpenBracket: Align
 */

static const struct bcmgenet_stats bcmgenet_gstrings_stats[] = {
	/* Misc UniMAC counters */
	STAT_GENET_MISC("rbuf_ovflow_cnt", mib.rbuf_ovflow_cnt,
			UMAC_RBUF_OVFL_CNT_V1),
	STAT_GENET_MISC("rbuf_err_cnt", mib.rbuf_err_cnt,
			UMAC_RBUF_ERR_CNT_V1),
	STAT_GENET_MISC("mdf_err_cnt", mib.mdf_err_cnt, UMAC_MDF_ERR_CNT),
	STAT_GENET_SOFT_MIB("alloc_rx_buff_failed", mib.alloc_rx_buff_failed),
	STAT_GENET_SOFT_MIB("rx_dma_failed", mib.rx_dma_failed),
	STAT_GENET_SOFT_MIB("tx_dma_failed", mib.tx_dma_failed),
	STAT_GENET_SOFT_MIB("tx_realloc_tsb", mib.tx_realloc_tsb),
	STAT_GENET_SOFT_MIB("tx_realloc_tsb_failed",
			    mib.tx_realloc_tsb_failed),
};
