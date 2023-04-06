/*
 * Braces sense alignment.
 */

const char *
ofmt_fail_str(enum ofmt_fail f)
{
	static const char *const repr[] = {
		[ofmt_fail_none]                    = "ofmt_fail_none",
		[ofmt_fail_bad_setup]               = "ofmt_fail_bad_setup",
		[ofmt_fail_bad_state]               = "ofmt_fail_bad_state",
		[ofmt_fail_invalid_pholder]         = "ofmt_fail_invalid_pholder",
		[ofmt_fail_parse]                   = "ofmt_fail_parse",
		[ofmt_fail_subproc]                 = "ofmt_fail_subproc",
		[ofmt_fail_system]                  = "ofmt_fail_system",
		[ofmt_fail_too_long_evaluation]     = "ofmt_fail_too_long_evaluation",
		[ofmt_fail_too_long_spec]           = "ofmt_fail_too_long_spec",
		[ofmt_fail_too_many_args]           = "ofmt_fail_too_many_args",
		[ofmt_fail_too_many_pholders]       = "ofmt_fail_too_many_pholders",
		[ofmt_fail_undefined_resolver]      = "ofmt_fail_undefined_resolver",
	};
	return repr[f];
}
