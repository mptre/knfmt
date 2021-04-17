/*
 * Squares and fields in brace initializers.
 */

struct iovec	iovec[2] = {
	[0].iov_base	= &entry,
	[0].iov_len	= sizeof(entry),
	[1].iov_base	= (void *)str->bytes,
	[1].iov_len	= str->len,
};
