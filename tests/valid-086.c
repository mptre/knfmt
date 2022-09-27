/*
 * Cast expression followed by brace initializer.
 */

int
main(void)
{
	*atom = (struct diff_atom){
	    .root	= d,
	    .pos	= pos,
	    .at		= NULL,	/* atom data is not memory-mapped */
	    .len	= len,
	    .hash	= hash,
	};
}
