/*
 * TLS-side overlay (MBEDTLS_USER_CONFIG_FILE).
 * Crypto options belong in ns_crypto_user.h.
 *
 * ssl.h / ssl_ciphersuites.c still need the legacy cipher and ECP size
 * types after the PSA client config has been read.
 */
#ifndef NS_MBEDTLS_USER_H
#define NS_MBEDTLS_USER_H

#include "mbedtls/private/cipher.h"

#endif /* NS_MBEDTLS_USER_H */
