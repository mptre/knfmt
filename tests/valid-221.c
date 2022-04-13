/*
 * Align struct fields.
 */

struct token {
	int			 tk_refs;

	enum {
		ONE,
	} tk_flags;

	const char		*tk_str;

	struct token_list	 tk_prefixes;
};
