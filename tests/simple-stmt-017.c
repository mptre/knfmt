/*
 * Statement w/o braces regression.
 */

int
main(void)
{
	switch (0) {
	case Addressing_Method_E:
		if (opcode_has_vex_prefix(opcode))
			return arena_sprintf(s, "r%u/m%u", operand->os, operand->os);
		break;
	}
}
