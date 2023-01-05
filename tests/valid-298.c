/*
 * Nested brace initializers.
 */

int
main(void)
{
	return write((struct vec[]){
	    {
		1,
		2,
	    }, {
		3,
		4,
	    },
	});
}
