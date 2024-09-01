/*
 * Turn empty statement in switch default case into a break statement.
 */

int
main(void)
{
	switch (0) {
	case 0: ;
	default: ;
	}
}
