/*
 * Call expression, misplaced closing parenthesis.
 */

int
main(void)
{
	persist_file = persist_file_new(
	    feed_info->global_ctx->persist_dir,
	    feed_info->name
	);
}
