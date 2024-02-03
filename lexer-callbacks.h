struct arena_scope;
struct lexer;

struct lexer_callbacks {
	/*
	 * Read callback with the following semantics:
	 *
	 *     1. In case of encountering an error, NULL must be
	 *        returned.
	 *     2. Signalling the reach of end of file is done by
	 *        returning a token with type LEXER_EOF.
	 *     3. If none of the above occurs, the next consumed token
	 *        is assumed to be returned.
	 */
	struct token	*(*read)(struct lexer *, void *);

	/* Optional callback invoked once all tokens are read. */
	void		 (*done)(struct lexer *, void *);

	/*
	 * Allocate a new token from the given arena scope.
	 * Expected to be initialized using the given token.
	 */
	struct token	*(*alloc)(struct arena_scope *,
	    const struct token *);

	/*
	 * Serialize routine used to turn the given token into something
	 * human readable.
	 */
	const char	*(*serialize)(struct arena_scope *,
	    const struct token *);

	/* Opaque argument passed to callbacks. */
	void		*arg;
};
