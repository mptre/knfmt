/*
 * Braces alignment expression regression.
 */

const Opcode_Map opcode_map_2 = {
	[0x0] = {
		[0x0] = {
			{ EVEX | OS, VPSHUFB(Vx, Hx, Wx) },
			{ VEX | OS,  VPSHUFB(Vx, Hx, Wx) },
			{ OS,        PSHUFB(Vx, Wx) },
			{ NP,        PSHUFB(Pq, Qq) },
		},
	},
};
