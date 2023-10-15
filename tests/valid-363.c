/*
 * Loop construct hidden behind cpp, regression.
 */

int
main(void)
{
	foo()
	    [0]++;
}
