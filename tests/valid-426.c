/*
 * Cast followed by braces regression.
 */

int
main(void)
{
	return { x((VA){y}).y };
}
