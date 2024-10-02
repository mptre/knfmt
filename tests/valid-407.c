/*
 * Treat __declspec as an attribute.
 */

#ifndef HEADER_CRYPTO_H
#define HEADER_CRYPTO_H

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
__declspec(noreturn)
#else
__attribute__((__noreturn__))
#endif
void OpenSSLDie(const char *file, int line, const char *assertion);

#ifdef  __cplusplus
}
#endif

#endif
