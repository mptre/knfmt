/*
 * Braces sense alignment.
 */

struct vmm_reg_debug_info rflags_info[16] = {
	{ PSL_C,   "CF ", "cf "},
	{ PSL_PF,  "PF ", "pf "},
	{ PSL_AF,  "AF ", "af "},
	{ PSL_Z,   "ZF ", "zf "},
	{ PSL_N,   "SF ", "sf "},	/* sign flag */
	{ PSL_T,   "TF ", "tf "},
	{ PSL_I,   "IF ", "if "},
	{ PSL_D,   "DF ", "df "},
	{ PSL_V,   "OF ", "of "},	/* overflow flag */
	{ PSL_NT,  "NT ", "nt "},
	{ PSL_RF,  "RF ", "rf "},
	{ PSL_VM,  "VM ", "vm "},
	{ PSL_AC,  "AC ", "ac "},
	{ PSL_VIF, "VIF ", "vif "},
	{ PSL_VIP, "VIP ", "vip "},
	{ PSL_ID,  "ID ", "id "},
};
