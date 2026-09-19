# Command-line build for the STM32H5F4 CubeIDE NS smoke app.
# Matches STM32CubeIDE/.cproject (MCU STM32H5F4xx, BL2_TRAILER_SIZE=0x3000,
# mbedtls-4.1.1 TLS 1.3 client helpers; PSA crypto still via SPE veneers;
# memory_buffer_alloc.c is compiled as the mbedtls heap; threading.c is not).
#
#   make
#   make clean
#
# Output: build/tfm_ns.elf  build/tfm_ns.bin
# Sign:  ./sign_kit/sign.sh build/tfm_ns.bin
#        (this kit writes tfm_ns_signed.bin into sign_kit/, not build/)
# Flash with windows-tfm-tools or ./flash_stm32h5f4.sh.

CROSS   ?= arm-none-eabi-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE    := $(CROSS)size

BUILD   := build
TARGET  := $(BUILD)/tfm_ns
LDSCRIPT := spe/out/appli_ns.pp.ld
VENEERS := spe/api_ns/interface/lib/s_veneers.o
MBEDTLS := ns_app/mbedtls-4.1.1

CPU := -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard

CPPFLAGS := \
	-DUSE_HAL_DRIVER \
	-DSTM32H5F4xx \
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
	-DTARGET_CONFIG_HEADER_FILE='"config_tfm_target.h"' \
	-DBL2 \
	-DBL2_HEADER_SIZE=0x400 \
	-DBL2_TRAILER_SIZE=0x3000 \
	-DMCUBOOT_IMAGE_NUMBER=2 \
	-DTFM_NS_LOG \
	-DNDEBUG \
	-DTF_PSA_CRYPTO_USER_CONFIG_FILE='"ns_crypto_user.h"' \
	-DMBEDTLS_USER_CONFIG_FILE='"ns_mbedtls_user.h"' \
	-DMBEDTLS_ALLOW_PRIVATE_ACCESS \
	-Ins_app \
	-I$(MBEDTLS)/include \
	-I$(MBEDTLS)/tf-psa-crypto/include \
	-I$(MBEDTLS)/library \
	-I$(MBEDTLS)/tf-psa-crypto/core \
	-I$(MBEDTLS)/tf-psa-crypto/drivers/builtin/include \
	-I$(MBEDTLS)/tf-psa-crypto/drivers/builtin/src \
	-I$(MBEDTLS)/tf-psa-crypto/extras \
	-I$(MBEDTLS)/tf-psa-crypto/platform \
	-I$(MBEDTLS)/tf-psa-crypto/utilities \
	-Ispe/api_ns/interface/include \
	-Ispe/api_ns/interface/include/crypto_keys \
	-Ispe/api_ns/platform/include \
	-Ispe/api_ns/platform/boards \
	-Ispe/api_ns/platform/Device/Include \
	-Ispe/api_ns/platform/ext/cmsis/Include \
	-Ispe/api_ns/platform/ext/cmsis/Include/m-profile \
	-Ispe/api_ns/platform/ext/common \
	-Ispe/api_ns/platform/hal/Inc

CFLAGS := $(CPU) -O2 -ffunction-sections -fdata-sections -Wall -fno-builtin

LDFLAGS := $(CPU) -T$(LDSCRIPT) -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs -Wl,-Map=$(TARGET).map

NS_APP_SRCS := \
	ns_app/main.c \
	ns_app/ns_mbedtls_bio.c \
	ns_app/tfm_log_raw.c \
	ns_app/syscalls_stub.c \
	ns_app/platform_init_def.c

TFM_SRCS := $(wildcard ns_app/tfm_src/*.c)

# Same exclusions as STM32CubeIDE/.cproject sourceEntries.
MBEDTLS_LIB_EXCL := mbedtls_config.c net_sockets.c pkcs7.c ssl_cookie.c \
	ssl_tls12_server.c ssl_tls13_server.c timing.c
MBEDTLS_EXTRAS_EXCL := lmots.c lms.c nist_kw.c
MBEDTLS_PLATFORM_EXCL := threading.c
MBEDTLS_UTIL_EXCL := pkcs5.c

MBEDTLS_SRCS := \
	$(filter-out $(addprefix $(MBEDTLS)/library/,$(MBEDTLS_LIB_EXCL)),$(wildcard $(MBEDTLS)/library/*.c)) \
	$(filter-out $(addprefix $(MBEDTLS)/tf-psa-crypto/extras/,$(MBEDTLS_EXTRAS_EXCL)),$(wildcard $(MBEDTLS)/tf-psa-crypto/extras/*.c)) \
	$(filter-out $(addprefix $(MBEDTLS)/tf-psa-crypto/platform/,$(MBEDTLS_PLATFORM_EXCL)),$(wildcard $(MBEDTLS)/tf-psa-crypto/platform/*.c)) \
	$(filter-out $(addprefix $(MBEDTLS)/tf-psa-crypto/utilities/,$(MBEDTLS_UTIL_EXCL)),$(wildcard $(MBEDTLS)/tf-psa-crypto/utilities/*.c)) \
	$(wildcard $(MBEDTLS)/tf-psa-crypto/drivers/builtin/src/*.c) \
	$(MBEDTLS)/tf-psa-crypto/core/psa_util.c

SRCS := $(NS_APP_SRCS) $(TFM_SRCS) $(MBEDTLS_SRCS)
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all clean size

all: $(TARGET).bin size

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS) $(VENEERS) $(LDSCRIPT)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(VENEERS)

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

size: $(TARGET).elf
	$(SIZE) -A $<

clean:
	rm -rf $(BUILD)
