/*
 * Functions returning function pointers.
 */

void	one();
int	(*two(void))(void);

int
(*SSL_get_verify_callback(const SSL *s))(int, X509_STORE_CTX *)
{
	return (s->internal->verify_callback);
}
