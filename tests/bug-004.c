/*
 * OpenSSL macro return type.
 */

const STACK_OF(X509_EXTENSION)*
X509_REVOKED_get0_extensions(const X509_REVOKED *x)
{
	return x->extensions;
}
