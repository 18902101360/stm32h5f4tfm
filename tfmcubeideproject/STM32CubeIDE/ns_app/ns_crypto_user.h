/*
 * Overlay on SPE's mbedtls/tf_psa_crypto_config.h (TF_PSA_CRYPTO_USER_CONFIG_FILE).
 *
 * SPE's client-only config keeps PSA_WANT_* in sync with the flashed Crypto
 * partition, but omits TLS/X.509 helper modules. Enable those here without
 * compiling a second PSA crypto core (MBEDTLS_PSA_CRYPTO_C stays off).
 */
#ifndef NS_CRYPTO_USER_H
#define NS_CRYPTO_USER_H

#define MBEDTLS_PSA_CRYPTO_CLIENT
#undef  MBEDTLS_PSA_CRYPTO_C

/*
 * Do not define MBEDTLS_ECP_LIGHT here: it requires MBEDTLS_ECP_DP_* and
 * pulls software ECP. ecp.h then sets MBEDTLS_ECP_MAX_BITS to a dummy 1,
 * which is enough for ssl_setup smoke. A real ECDHE handshake needs a
 * larger premaster (enable a curve / PSA ECDHE, still without compiling
 * builtin ecp.c / aes.c).
 */

#define MBEDTLS_CIPHER_C
#define MBEDTLS_MD_C
#define MBEDTLS_OID_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_ERROR_C
#define MBEDTLS_PLATFORM_C

#endif /* NS_CRYPTO_USER_H */
