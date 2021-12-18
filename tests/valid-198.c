/*
 * Trailing whitespace in comment.
 */

int
main(void)
{
#ifndef IEEE80211_STA_ONLY
	/* 
	 * In a process context, try to add this node to the
	 * firmware table and confirm the AUTH request.
	 */
	return EBUSY;
#else
	return 0;
#endif /* IEEE80211_STA_ONLY */
}
