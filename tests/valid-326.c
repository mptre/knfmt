/*
 * Excessive new lines in brace initializers.
 */

static const ASN1_ADB_TABLE X9_62_CHARACTERISTIC_TWO_adbtbl[] = {
	{
		.value = NID_X9_62_onBasis,
		.tt = {
			.flags = 0,
			.tag = 0,
			.offset = offsetof(X9_62_CHARACTERISTIC_TWO, p.onBasis),
			.field_name = "p.onBasis",
			.item = &ASN1_NULL_it,
		},

	},
	{
		.value = NID_X9_62_tpBasis,
		.tt = {
			.flags = 0,
			.tag = 0,
			.offset = offsetof(X9_62_CHARACTERISTIC_TWO, p.tpBasis),
			.field_name = "p.tpBasis",
			.item = &ASN1_INTEGER_it,
		},

	},
	{
		.value = NID_X9_62_ppBasis,
		.tt = {
			.flags = 0,
			.tag = 0,
			.offset = offsetof(X9_62_CHARACTERISTIC_TWO, p.ppBasis),
			.field_name = "p.ppBasis",
			.item = &X9_62_PENTANOMIAL_it,
		},

	},
};
