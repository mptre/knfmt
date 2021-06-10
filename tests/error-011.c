/*
 * Confusing return type.
 */

EVP_PKEY *
load_pubkey(BIO *err, const char *file, int format, int maybe_stdin,
    const char *pass, const char *key_descrip)
{
	if (format == FORMAT_ASN1) {
		pkey = d2i_PUBKEY_bio(key, NULL);
	}
#if !defined(OPENSSL_NO_RC4) && !defined(OPENSSL_NO_RSA)
	else if (format == FORMAT_NETSCAPE || format == FORMAT_IISSGC)
		pkey = load_netscape_key(err, key, file, key_descrip, format);
#endif
#if !defined(OPENSSL_NO_RSA) && !defined(OPENSSL_NO_DSA)
	else if (format == FORMAT_MSBLOB)
		pkey = b2i_PublicKey_bio(key);
#endif
	else {
		BIO_printf(err, "bad input format specified for key file\n");
		goto end;
	}
}

STACK_OF(X509) *
load_certs(BIO *err, const char *file, int format, const char *pass,
    const char *desc)
{
	STACK_OF(X509) *certs;

	if (!load_certs_crls(err, file, format, pass, desc, &certs, NULL))
		return NULL;
	return certs;
}
