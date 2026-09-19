/*
 * Bare-metal NS smoke test for STM32H573I-DK + TF-M SPE.
 *
 * Bring-up follows tf-m-tests/app_broker/main_ns.c (without RTX):
 *   tfm_ns_platform_init() -> stdio_init()
 *   tfm_ns_cp_init()
 *   tfm_ns_interface_init()
 *
 * Logging matches tf-m-tests: LOG_MSG -> tfm_log_printf -> stdio_output_string.
 * Do not use newlib printf (official NS tests never do).
 *
 * All other C files come from TF-M or tf-m-tests (or the SPE api_ns export).
 *
 * Requires the matching flashed tfm_s.bin (s_veneers.o addresses must match).
 * USART1 / ST-Link VCP: 115200 8N1, JP1 not fitted.
 *
 * If SPE was built with TEST_S=ON, colored "PASSED" / "*** End of Secure
 * test suites ***" prints first. This app then prints NS-SMOKE.
 */

#include <string.h>
#include <stdint.h>

#include "Driver_USART.h"
#include "tfm_plat_ns.h"
#include "tfm_ns_interface.h"
#include "os_wrapper/common.h"
#include "test_log.h"

#include "psa/crypto.h"
#include "psa/error.h"
#include "psa/internal_trusted_storage.h"
#include "psa/update.h"

#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/version.h"

static int g_fail;

/* tfm_log_printf has no %02x; print two lowercase hex digits per byte. */
static void log_hex(const uint8_t *buf, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        LOG_MSG("%x%x", (unsigned)((buf[i] >> 4) & 0xfu),
                (unsigned)(buf[i] & 0xfu));
    }
}

static void check(const char *what, psa_status_t status)
{
    if (status == PSA_SUCCESS) {
        LOG_MSG("  [PASS] %s\r\n", what);
    } else {
        LOG_MSG("  [FAIL] %s status=%d\r\n", what, (int)status);
        g_fail++;
    }
}

static void test_crypto(void)
{
    static const uint8_t msg[] = "abc";
    /* FIPS 180-2 SHA-256("abc") */
    static const uint8_t expect[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    uint8_t hash[32];
    size_t hash_len = 0;
    psa_status_t status;

    LOG_MSG("PSA Crypto\r\n");
    status = psa_crypto_init();
    check("psa_crypto_init", status);
    if (status != PSA_SUCCESS) {
        return;
    }

    status = psa_hash_compute(PSA_ALG_SHA_256, msg, sizeof(msg) - 1u,
                              hash, sizeof(hash), &hash_len);
    check("psa_hash_compute(SHA-256)", status);
    if (status == PSA_SUCCESS) {
        LOG_MSG("  hash=");
        log_hex(hash, hash_len);
        LOG_MSG("\r\n");
        if ((hash_len != sizeof(expect)) ||
            (memcmp(hash, expect, sizeof(expect)) != 0)) {
            LOG_MSG("  [FAIL] SHA-256 known-answer mismatch\r\n");
            g_fail++;
        } else {
            LOG_MSG("  [PASS] SHA-256 known-answer\r\n");
        }
    }
}

static void test_its(void)
{
    const psa_storage_uid_t uid = 0x0000000000001001ULL;
    static const uint8_t payload[] = "ns-its";
    uint8_t readback[16];
    size_t read_len = 0;
    psa_status_t status;

    LOG_MSG("PSA ITS\r\n");
    (void)psa_its_remove(uid);

    status = psa_its_set(uid, sizeof(payload), payload, PSA_STORAGE_FLAG_NONE);
    check("psa_its_set", status);

    memset(readback, 0, sizeof(readback));
    status = psa_its_get(uid, 0, sizeof(readback), readback, &read_len);
    check("psa_its_get", status);
    if ((status == PSA_SUCCESS) &&
        ((read_len != sizeof(payload)) ||
         (memcmp(readback, payload, sizeof(payload)) != 0))) {
        LOG_MSG("  [FAIL] ITS payload mismatch\r\n");
        g_fail++;
    }

    status = psa_its_remove(uid);
    check("psa_its_remove", status);
}

static void test_tls_config(void)
{
    mbedtls_ssl_config conf;
    mbedtls_ssl_context ssl;
    int ret;

    LOG_MSG("Mbed TLS %s (PSA client)\r\n", MBEDTLS_VERSION_STRING);

    mbedtls_ssl_config_init(&conf);
    mbedtls_ssl_init(&ssl);

    ret = mbedtls_ssl_config_defaults(&conf,
                                      MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    check("mbedtls_ssl_config_defaults",
          (ret == 0) ? PSA_SUCCESS : PSA_ERROR_GENERIC_ERROR);
    if (ret != 0) {
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        return;
    }

    mbedtls_ssl_conf_min_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);

    ret = mbedtls_ssl_setup(&ssl, &conf);
    check("mbedtls_ssl_setup TLS1.2-1.3",
          (ret == 0) ? PSA_SUCCESS : PSA_ERROR_GENERIC_ERROR);

    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
}

static void test_fwu_query(void)
{
    psa_fwu_component_info_t info;
    psa_status_t status;

    LOG_MSG("PSA FWU query\r\n");

    memset(&info, 0, sizeof(info));
    status = psa_fwu_query(FWU_COMPONENT_ID_SECURE, &info);
    check("psa_fwu_query(S)", status);
    if (status == PSA_SUCCESS) {
        LOG_MSG("  S  state=%u max_size=%u\r\n",
                (unsigned)info.state, (unsigned)info.max_size);
    }

    memset(&info, 0, sizeof(info));
    status = psa_fwu_query(FWU_COMPONENT_ID_NONSECURE, &info);
    check("psa_fwu_query(NS)", status);
    if (status == PSA_SUCCESS) {
        LOG_MSG("  NS state=%u max_size=%u\r\n",
                (unsigned)info.state, (unsigned)info.max_size);
    }
}

int main(void)
{
    if (tfm_ns_platform_init() != ARM_DRIVER_OK) {
        for (;;) {
        }
    }

    if (tfm_ns_cp_init() != ARM_DRIVER_OK) {
        for (;;) {
        }
    }

    LOG_MSG("\r\nNS-SMOKE klp\r\n");
    LOG_MSG("Non-Secure system starting...\r\n");

    if (tfm_ns_interface_init() != OS_WRAPPER_SUCCESS) {
        LOG_MSG("tfm_ns_interface_init failed\r\n");
        for (;;) {
        }
    }
    LOG_MSG("tfm_ns_interface_init ok\r\n");

    test_crypto();
    test_tls_config();
    test_its();
    test_fwu_query();

    if (g_fail == 0) {
        LOG_MSG("ALL PASSED\r\n");
    } else {
        LOG_MSG("FAILED count=%d\r\n", g_fail);
    }

    for (;;) {
    }
}
