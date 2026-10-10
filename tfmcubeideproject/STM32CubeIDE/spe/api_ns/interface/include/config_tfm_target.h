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

/*
 * TLS 1.3 on NS needs a larger Crypto IPC scratch. Default 5120 is too small
 * for psa_export_public_key() during the handshake (PSA_ERROR_INSUFFICIENT_MEMORY
 * -141 at ssl_tls13_generic.c). This is NOT the mbedTLS engine heap.
 */
#define CRYPTO_IOVEC_BUFFER_SIZE  20480

/*
 * TF-PSA-Crypto engine heap (mbedtls_mem_buf in crypto_library.c). Default
 * 0x3000 (12 KiB) is only enough for one RSA-4096 verify while the heap is
 * empty. Each live TLS 1.3 session keeps PSA keys/contexts in this buffer, so
 * a second WSS handshake fails at psa_verify_hash() with -141 while walking a
 * Let's Encrypt chain (Root YR / ISRG Root X1 are RSA-4096). mbedTLS then
 * reports MBEDTLS_ERR_X509_CERT_VERIFY_FAILED (-0x2700) / BADCERT_NOT_TRUSTED
 * because x509_crt_find_parent_in() treats any non-zero check_signature() as
 * a bad signature. 32 KiB covers two concurrent sessions plus one RSA-4096
 * verify (and reconnect overlap). S data RAM is ~128 KiB.
 */
#undef CRYPTO_ENGINE_BUF_SIZE
#define CRYPTO_ENGINE_BUF_SIZE                 0x8000

/*
 * Storage objects vs flash:
 * ITS 16 KB, 2×8 KB blocks (metadata+data share a block). 12 × 512 B all fit.
 * PS 64 KB, 8×8 KB blocks. PS_ENCRYPTION on; a 2048 B asset is ~2.1 KB on flash.
 * 24 slots pass FS checks; ~22 full-size assets can coexist (table + header).
 */
#define ITS_MAX_ASSET_SIZE                     512
#define ITS_NUM_ASSETS                         12
#define PS_MAX_ASSET_SIZE                      2048
#define PS_NUM_ASSETS                          24

#endif /* __CONFIG_TFM_TARGET_H__ */
