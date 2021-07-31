/*
 * Cast expression with unknown type.
 */

int
main(void)
{
	placeholders_set_extra(gctx->ofmt, &(const placeholder_extra_t){
		.incremental_id = *(pc->extra_pholders.incremental_id),
	});
}
