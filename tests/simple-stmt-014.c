/*
 * Missing curly braces around single statement spanning multiple lines.
 */

int
main(void)
{
	if (0) {
		for (unsigned int i = 0; i < VECTOR_LENGTH(parts); i++)
			fprintf(stderr, "XXX %s: parts '%s'\n", __func__, parts[i]);
	}
}
