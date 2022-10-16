/*
 * AlignAfterOpenBracket: Align
 */

int
main(void)
{
	return download_schedule(
	    feed_info->global_ctx->dload,
	    feed_info->config->url,
	    on_downloaded,
	    feed_info);
}
