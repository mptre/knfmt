/*
 * queue(3) macro confused as declaration.
 */

int
main(void)
{
	TAILQ_FOREACH(pe, paths, entry)
		sorted_entries[i++] = pe->data;
	mergesort(sorted_entries, nentries, sizeof(struct got_tree_entry *),
	    sort_tree_entries_the_way_git_likes_it);
}
