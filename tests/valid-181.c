/*
 * Optional line regression.
 */

int
main(void)
{
	if (nest) {
		if (parser_exec_decl_braces_fields(pr, concat, rl, rbrace,
		    PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_ENUM | PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_TRIM))
			return parser_error(pr);
	}
}
