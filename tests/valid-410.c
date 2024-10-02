/*
 * Aligned rows in nested brace initializers.
 */

static const X86_Opcode opcode_map_extension[17][2][4][8] = {
	[0x01] = {
		[0x0] = {
			[0x0] = { ADD_1A, OR_1A,  ADC_1A, SBB_1A,
				  AND_1A, SUB_1A, XOR_1A, CMP_1A },
		},
	},
};
