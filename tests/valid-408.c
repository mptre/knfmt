/*
 * Aligned rows in nested brace initializers.
 */

static const Opcode opcode_map_00_f7[16][8] = {
	[0x00] = { ADD(Eb, Gb), ADD(Ev, Gv),  ADD(Gb, Eb),  ADD(Gv, Ev),
		   ADD(AL, Ib), ADD(rAX, Iz), PUSH_i64(ES), POP_i64(ES) },
};
