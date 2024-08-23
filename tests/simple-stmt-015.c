/*
 * Regression in simple stmt/stmt-empty-loop interaction.
 */

int
main(void)
{
	while (0) {
		if (0) {
			x = 1;
		}
	}
}
