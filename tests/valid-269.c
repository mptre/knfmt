/*
 * Long return expression.
 */

static int
doc_diff_is_mute(const struct doc_state *st)
{
	return DOC_DIFF(st) &&
	    (st->st_diff.end == 0 || st->st_diff.verbatim != NULL);
}
