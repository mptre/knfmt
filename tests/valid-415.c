/*
 * Designated initializer followed by expression.
 */

int
main(void)
{
	return (Modrm){.mod = mod, .reg = reg, .rm = rm}.u8[0];
}
