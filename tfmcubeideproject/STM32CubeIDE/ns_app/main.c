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
#include "mbedtls/x509_csr.h"
#include "mbedtls/pk.h"
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

static void check_mbed(const char *what, int ret)
{
    if (ret == 0) {
        LOG_MSG("  [PASS] %s\r\n", what);
    } else {
        LOG_MSG("  [FAIL] %s ret=%d\r\n", what, ret);
        g_fail++;
    }
}

/*
 * PKCS#10 write + parse, then issue a CRT with a SPE-resident CA key.
 * Leaf/CA private keys stay in Crypto (mbedtls_pk_wrap_psa).
 */
static void test_csr(void)
{
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    mbedtls_svc_key_id_t leaf_id = MBEDTLS_SVC_KEY_ID_INIT;
    mbedtls_svc_key_id_t ca_id = MBEDTLS_SVC_KEY_ID_INIT;
    mbedtls_pk_context leaf_pk;
    mbedtls_pk_context ca_pk;
    mbedtls_x509write_csr csr;
    mbedtls_x509_csr parsed;
    mbedtls_x509write_cert crt;
    mbedtls_x509_crt parsed_crt;
    static unsigned char csr_der[512];
    static unsigned char csr_pem[768];
    static unsigned char crt_der[1024];
    static unsigned char crt_pem[1536];
    static const unsigned char serial[] = { 0x01 };
    char subject[128];
    const unsigned char *csr_der_p;
    psa_status_t status;
    int ret;
    int csr_der_len;
    int crt_der_len;

    LOG_MSG("Mbed TLS CSR parse / CRT write\r\n");

    mbedtls_pk_init(&leaf_pk);
    mbedtls_pk_init(&ca_pk);
    mbedtls_x509write_csr_init(&csr);
    mbedtls_x509_csr_init(&parsed);
    mbedtls_x509write_crt_init(&crt);
    mbedtls_x509_crt_init(&parsed_crt);

    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_HASH);
    psa_set_key_algorithm(&attr, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attr, 256);

    status = psa_generate_key(&attr, &leaf_id);
    check("psa_generate_key(leaf P-256)", status);
    if (status != PSA_SUCCESS) {
        goto cleanup;
    }

    ret = mbedtls_pk_wrap_psa(&leaf_pk, leaf_id);
    check_mbed("mbedtls_pk_wrap_psa(leaf)", ret);
    if (ret != 0) {
        goto cleanup;
    }

    mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);
    mbedtls_x509write_csr_set_key(&csr, &leaf_pk);
    ret = mbedtls_x509write_csr_set_subject_name(&csr, "CN=stm32h573-ns,O=tfm");
    check_mbed("mbedtls_x509write_csr_set_subject_name", ret);
    if (ret != 0) {
        goto cleanup;
    }

    csr_der_len = mbedtls_x509write_csr_der(&csr, csr_der, sizeof(csr_der));
    if (csr_der_len > 0) {
        LOG_MSG("  [PASS] mbedtls_x509write_csr_der len=%d\r\n", csr_der_len);
        csr_der_p = csr_der + sizeof(csr_der) - csr_der_len;
    } else {
        LOG_MSG("  [FAIL] mbedtls_x509write_csr_der ret=%d\r\n", csr_der_len);
        g_fail++;
        goto cleanup;
    }

    memset(csr_pem, 0, sizeof(csr_pem));
    ret = mbedtls_x509write_csr_pem(&csr, csr_pem, sizeof(csr_pem));
    check_mbed("mbedtls_x509write_csr_pem", ret);

    ret = mbedtls_x509_csr_parse_der(&parsed, csr_der_p, (size_t)csr_der_len);
    check_mbed("mbedtls_x509_csr_parse_der", ret);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_x509_dn_gets(subject, sizeof(subject), &parsed.subject);
    if (ret >= 0) {
        LOG_MSG("  [PASS] CSR subject\r\n");
        LOG_MSG("  %s\r\n", subject);
    } else {
        check_mbed("mbedtls_x509_dn_gets", ret);
        goto cleanup;
    }

    /* Leaf private key is no longer needed; subject public key is in parsed. */
    mbedtls_x509write_csr_free(&csr);
    mbedtls_x509write_csr_init(&csr);
    mbedtls_pk_free(&leaf_pk);
    mbedtls_pk_init(&leaf_pk);
    (void)psa_destroy_key(leaf_id);
    leaf_id = MBEDTLS_SVC_KEY_ID_INIT;

    status = psa_generate_key(&attr, &ca_id);
    check("psa_generate_key(CA P-256)", status);
    if (status != PSA_SUCCESS) {
        goto cleanup;
    }

    ret = mbedtls_pk_wrap_psa(&ca_pk, ca_id);
    check_mbed("mbedtls_pk_wrap_psa(CA)", ret);
    if (ret != 0) {
        goto cleanup;
    }

    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, &parsed.pk);
    mbedtls_x509write_crt_set_issuer_key(&crt, &ca_pk);
    ret = mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof(serial));
    check_mbed("mbedtls_x509write_crt_set_serial_raw", ret);
    ret = mbedtls_x509write_crt_set_validity(&crt, "20260101000000", "20361231235959");
    check_mbed("mbedtls_x509write_crt_set_validity", ret);
    ret = mbedtls_x509write_crt_set_issuer_name(&crt, "CN=stm32h573-ca,O=tfm");
    check_mbed("mbedtls_x509write_crt_set_issuer_name", ret);
    ret = mbedtls_x509write_crt_set_subject_name(&crt, "CN=stm32h573-ns,O=tfm");
    check_mbed("mbedtls_x509write_crt_set_subject_name", ret);
    ret = mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1);
    check_mbed("mbedtls_x509write_crt_set_basic_constraints", ret);

    crt_der_len = mbedtls_x509write_crt_der(&crt, crt_der, sizeof(crt_der));
    if (crt_der_len > 0) {
        LOG_MSG("  [PASS] mbedtls_x509write_crt_der len=%d\r\n", crt_der_len);
    } else {
        LOG_MSG("  [FAIL] mbedtls_x509write_crt_der ret=%d\r\n", crt_der_len);
        g_fail++;
        goto cleanup;
    }

    memset(crt_pem, 0, sizeof(crt_pem));
    ret = mbedtls_x509write_crt_pem(&crt, crt_pem, sizeof(crt_pem));
    check_mbed("mbedtls_x509write_crt_pem", ret);

    ret = mbedtls_x509_crt_parse_der(&parsed_crt,
                                     crt_der + sizeof(crt_der) - crt_der_len,
                                     (size_t)crt_der_len);
    check_mbed("mbedtls_x509_crt_parse_der", ret);

cleanup:
    psa_reset_key_attributes(&attr);
    mbedtls_x509write_csr_free(&csr);
    mbedtls_x509write_crt_free(&crt);
    mbedtls_x509_csr_free(&parsed);
    mbedtls_x509_crt_free(&parsed_crt);
    mbedtls_pk_free(&leaf_pk);
    mbedtls_pk_free(&ca_pk);
    (void)psa_destroy_key(leaf_id);
    (void)psa_destroy_key(ca_id);
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
    test_csr();
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
