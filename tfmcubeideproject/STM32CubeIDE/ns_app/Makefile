# Command-line build of the CubeIDE NS app + Mbed TLS 4.1.1 (PSA client).
# Run from this directory: make -j$(nproc)
#
# Matches STM32CubeIDE Debug flags: Cortex-M33, fpv5-sp-d16, hard-float.

CROSS ?= arm-none-eabi-
CC    := $(CROSS)gcc
LD    := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE  := $(CROSS)size

ROOT     := $(abspath .)
CUBE     := $(abspath ..)
SPE      := $(CUBE)/spe
API      := $(SPE)/api_ns
MBEDTLS  := $(ROOT)/mbedtls-4.1.1
OUT      := $(ROOT)/out

CPUFLAGS := -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
SPECS    := --specs=nano.specs --specs=nosys.specs

DEFINES := \
	-DSTM32H573xx \
	-DUSE_HAL_DRIVER \
	-DDOMAIN_NS=1 \
	-DCONFIG_TFM_FLOAT_ABI=2 \
	-DCONFIG_TFM_ENABLE_CP10CP11 \
	-DPLATFORM_DEFAULT_CRYPTO_KEYS \
	-DCONFIG_TFM_USE_TRUSTZONE \
	-DTFM_ISOLATION_LEVEL=1 \
	-DTFM_PARTITION_CRYPTO \
	-DTFM_PARTITION_INTERNAL_TRUSTED_STORAGE \
	-DTFM_PARTITION_PROTECTED_STORAGE \
	-DTFM_PARTITION_FIRMWARE_UPDATE \
	-DTFM_PARTITION_INITIAL_ATTESTATION \
	-DTFM_PARTITION_PLATFORM \
	-DTFM_PSA_CRYPTO_CLIENT_ONLY \
	-DTF_PSA_CRYPTO_CONFIG_FILE='"mbedtls/tf_psa_crypto_config.h"' \
	-DTF_PSA_CRYPTO_USER_CONFIG_FILE='"ns_crypto_user.h"' \
	-DMBEDTLS_USER_CONFIG_FILE='"ns_mbedtls_user.h"' \
	-DTARGET_CONFIG_HEADER_FILE='"config_tfm_target.h"' \
	-DBL2 \
	-DBL2_HEADER_SIZE=0x400 \
	-DBL2_TRAILER_SIZE=0x2000 \
	-DMCUBOOT_IMAGE_NUMBER=2 \
	-DTFM_NS_LOG \
	-DNDEBUG \
	-DMBEDTLS_ALLOW_PRIVATE_ACCESS

INCLUDES := \
	-I$(ROOT) \
	-I$(MBEDTLS)/include \
	-I$(MBEDTLS)/tf-psa-crypto/include \
	-I$(MBEDTLS)/library \
	-I$(MBEDTLS)/tf-psa-crypto/core \
	-I$(MBEDTLS)/tf-psa-crypto/drivers/builtin/include \
	-I$(MBEDTLS)/tf-psa-crypto/drivers/builtin/src \
	-I$(MBEDTLS)/tf-psa-crypto/extras \
	-I$(MBEDTLS)/tf-psa-crypto/platform \
	-I$(MBEDTLS)/tf-psa-crypto/utilities \
	-I$(API)/interface/include \
	-I$(API)/interface/include/crypto_keys \
	-I$(API)/platform/include \
	-I$(API)/platform/boards \
	-I$(API)/platform/Device/Include \
	-I$(API)/platform/ext/cmsis/Include \
	-I$(API)/platform/ext/cmsis/Include/m-profile \
	-I$(API)/platform/ext/common \
	-I$(API)/platform/hal/Inc

CFLAGS := $(CPUFLAGS) -std=gnu11 -g3 -O1 \
	-ffunction-sections -fdata-sections -Wall \
	$(DEFINES) $(INCLUDES)

LDFLAGS := $(CPUFLAGS) $(SPECS) -Wl,--gc-sections \
	-T$(SPE)/out/appli_ns.pp.ld \
	-Wl,--start-group -lc -lm -lgcc -Wl,--end-group

APP_SRCS := \
	$(ROOT)/main.c \
	$(ROOT)/platform_init_def.c \
	$(ROOT)/syscalls_stub.c \
	$(ROOT)/tfm_log_raw.c \
	$(wildcard $(ROOT)/tfm_src/*.c)

MBEDTLS_LIB_EXCLUDE := \
	net_sockets.c mbedtls_config.c timing.c pkcs7.c \
	ssl_tls12_server.c ssl_tls13_server.c ssl_cookie.c \
	x509write.c x509write_crt.c x509write_csr.c x509_create.c x509_csr.c

MBEDTLS_LIB_SRCS := $(filter-out \
	$(addprefix $(MBEDTLS)/library/,$(MBEDTLS_LIB_EXCLUDE)), \
	$(wildcard $(MBEDTLS)/library/*.c))

CRYPTO_SUPPORT_SRCS := \
	$(MBEDTLS)/tf-psa-crypto/utilities/asn1parse.c \
	$(MBEDTLS)/tf-psa-crypto/utilities/asn1write.c \
	$(MBEDTLS)/tf-psa-crypto/utilities/base64.c \
	$(MBEDTLS)/tf-psa-crypto/utilities/oid.c \
	$(MBEDTLS)/tf-psa-crypto/utilities/pem.c \
	$(MBEDTLS)/tf-psa-crypto/utilities/constant_time.c \
	$(MBEDTLS)/tf-psa-crypto/platform/platform.c \
	$(MBEDTLS)/tf-psa-crypto/platform/platform_util.c \
	$(MBEDTLS)/tf-psa-crypto/extras/md.c \
	$(MBEDTLS)/tf-psa-crypto/extras/pk.c \
	$(MBEDTLS)/tf-psa-crypto/extras/pkparse.c \
	$(MBEDTLS)/tf-psa-crypto/extras/pk_ecc.c \
	$(MBEDTLS)/tf-psa-crypto/extras/pk_rsa.c \
	$(MBEDTLS)/tf-psa-crypto/extras/pk_wrap.c \
	$(MBEDTLS)/tf-psa-crypto/drivers/builtin/src/psa_util_internal.c

APP_OBJS  := $(patsubst $(ROOT)/%.c,$(OUT)/obj/%.o,$(APP_SRCS))
MBED_OBJS := $(patsubst $(MBEDTLS)/%.c,$(OUT)/obj/mbedtls/%.o,$(MBEDTLS_LIB_SRCS) $(CRYPTO_SUPPORT_SRCS))
OBJS      := $(APP_OBJS) $(MBED_OBJS)

.PHONY: all clean tls-objects
all: $(OUT)/tfm_ns.elf $(OUT)/tfm_ns.bin

tls-objects: $(filter $(OUT)/obj/mbedtls/%,$(OBJS))
	@echo "Mbed TLS objects built: $^" | wc -w

$(OUT)/tfm_ns.elf: $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(OBJS) $(API)/interface/lib/s_veneers.o $(LDFLAGS) -o $@
	$(SIZE) $@

$(OUT)/tfm_ns.bin: $(OUT)/tfm_ns.elf
	$(OBJCOPY) -O binary $< $@

$(OUT)/obj/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/obj/mbedtls/%.o: $(MBEDTLS)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUT)
