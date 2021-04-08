/*
 * Long for loop.
 */

int
main(void)
{
	for (tmp_ptr = strpbrk(string, separators); tmp_ptr;
	    tmp_ptr = strpbrk(tmp_ptr, separators))
		ptr = tmp_ptr++;
}
