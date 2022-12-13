/*
 * Invalid switch case expression.
 */

int
main(void)
{
	switch (1) {
	case a[:
		break;
	}
}
