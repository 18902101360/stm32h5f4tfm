/*
 * Copyright (c) 2022-2024, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __CONFIG_TFM_TARGET_H__
#define __CONFIG_TFM_TARGET_H__

/* Use stored NV seed to provide entropy */
#undef CRYPTO_NV_SEED
#define CRYPTO_NV_SEED                         0

/* Use external RNG to provide entropy */
#define CRYPTO_EXT_RNG                         1

/* ../ns_app/mbedtls-4.1.1/library/ssl_tls13_generic.c:1621 0x2005a788: psa_export_public_key() returned -141  */
#define ITS_MAX_ASSET_SIZE                     512
#define ITS_NUM_ASSETS                         16
#define PS_MAX_ASSET_SIZE                      4096
#define PS_NUM_ASSETS                          120

/*
 * PS AEAD (GCM) copies plaintext and ciphertext+tag into the Crypto
 * partition scratch. Default CRYPTO_IOVEC_BUFFER_SIZE is 5120, which
 * fits PS_MAX_ASSET_SIZE 2048 but not 4096 (~4108 + 4 + 4124).
 * TFM_S_PS_TEST_1022 / TFM_NS_PS_TEST_1025 then fail on psa_ps_set.
 * 20480 matches stm32h573i_dk and leaves room for NS TLS/CSR PSA calls.
 */
#define CRYPTO_IOVEC_BUFFER_SIZE               20480

#endif /* __CONFIG_TFM_TARGET_H__ */
