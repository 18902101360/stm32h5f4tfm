# Bare-metal NS image for STM32H5F4, linked against TF-M SPE export.
#
# Supported layouts:
#   Makefile, main.c, api_ns/     (siblings)
#   ns_app/{Makefile,main.c}  +  ../trusted-firmware-m/build_s/api_ns
#
#   make
#   make SPE=/path/to/api_ns
#   make TOOLCHAIN_DIR=/path/to/arm-gnu-toolchain/bin
#   make clean
#
# CPU/ABI must match SPE: cortex-m33+nodsp, hard float, FPv5-SP-D16.
# MCU STM32H5F4xx, NS slot 0x0C0D0000, BL2_TRAILER_SIZE=0x3000.
# s_veneers.o is linked as a raw object (absolute NSC addresses).
#
# NS bring-up matches tf-m-tests/app_broker/main_ns.c without RTX.
# Logging matches official NS tests: LOG_MSG / tfm_log_raw.c / -DTFM_NS_LOG.
# Only main.c is local. Other sources are copied from TF-M / tf-m-tests
# or compiled from the SPE api_ns export.
#
# Flash the signed image with windows-tfm-tools or ./flash_stm32h5f4.sh.
# Signing uses sign_kit/ (H5F4 slot sizes, local venv). Standalone:
#   ./sign_kit/sign.sh out/tfm_ns.bin

NS_APP      := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
ROOT        := $(abspath $(NS_APP)/..)
ifeq ($(wildcard $(NS_APP)/api_ns/interface/lib/s_veneers.o),)
ifneq ($(wildcard $(ROOT)/trusted-firmware-m/build_s/api_ns/interface/lib/s_veneers.o),)
SPE         ?= $(ROOT)/trusted-firmware-m/build_s/api_ns
else
SPE         ?= $(NS_APP)/api_ns
endif
else
SPE         ?= $(NS_APP)/api_ns
endif
OUT         := $(NS_APP)/out
OBJDIR      := $(OUT)/obj

CROSS       ?= arm-none-eabi
TOOLCHAIN_DIR ?=
ifneq ($(TOOLCHAIN_DIR),)
export PATH := $(TOOLCHAIN_DIR):$(PATH)
endif

CC          := $(CROSS)-gcc
OBJCOPY     := $(CROSS)-objcopy
NM          := $(CROSS)-nm

# Do not default to the TF-M SPE .venv: a half-upgraded cryptography wheel
# (50.0.0 vs 50.0.1) makes imgtool crash in default_backend().
# h5f4_sign_python.sh uses python3, or creates tfmmakeproject/.sign-venv.
PYTHON      ?=

CPU         := -mcpu=cortex-m33+nodsp -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16

CPPFLAGS    := \
	-DSTM32H5F4xx \
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
	-DTARGET_CONFIG_HEADER_FILE='"config_tfm_target.h"' \
	-DBL2 \
	-DBL2_HEADER_SIZE=0x400 \
	-DBL2_TRAILER_SIZE=0x3000 \
	-DMCUBOOT_IMAGE_NUMBER=2 \
	-DTFM_NS_LOG \
	-I$(NS_APP) \
	-I$(SPE)/interface/include \
	-I$(SPE)/interface/include/crypto_keys \
	-I$(SPE)/platform/include \
	-I$(SPE)/platform/boards \
	-I$(SPE)/platform/Device/Include \
	-I$(SPE)/platform/ext/cmsis/Include \
	-I$(SPE)/platform/ext/cmsis/Include/m-profile \
	-I$(SPE)/platform/ext/common \
	-I$(SPE)/platform/hal/Inc

CFLAGS      := $(CPU) -std=gnu11 -g -ffunction-sections -fdata-sections \
	-fno-builtin -funsigned-char -Wall -DNDEBUG $(CPPFLAGS)

LDFLAGS     := $(CPU) -specs=nano.specs -specs=nosys.specs \
	-Wl,--gc-sections -Wl,--print-memory-usage \
	-Wl,-Map=$(OUT)/tfm_ns.map \
	-T $(OUT)/appli_ns.pp.ld

APP_SRCS := \
	$(NS_APP)/main.c \
	$(NS_APP)/syscalls_stub.c \
	$(NS_APP)/platform_init_def.c \
	$(NS_APP)/tfm_log_raw.c

SPE_SRCS := \
	$(SPE)/interface/src/os_wrapper/tfm_ns_interface_bare_metal.c \
	$(SPE)/interface/src/tfm_tz_psa_ns_api.c \
	$(SPE)/interface/src/tfm_crypto_api.c \
	$(SPE)/interface/src/tfm_its_api.c \
	$(SPE)/interface/src/tfm_ps_api.c \
	$(SPE)/interface/src/tfm_fwu_api.c \
	$(SPE)/interface/src/tfm_attest_api.c \
	$(SPE)/interface/src/tfm_platform_api.c \
	$(SPE)/platform/Device/Source/startup_stm32h5xx_ns.c \
	$(SPE)/platform/Device/Source/system_stm32h5xx.c \
	$(SPE)/platform/ext/common/uart_stdout.c \
	$(SPE)/platform/hal/CMSIS_Driver/low_level_com.c \
	$(SPE)/platform/hal/Src/stm32h5xx_hal.c \
	$(SPE)/platform/hal/Src/stm32h5xx_hal_cortex.c \
	$(SPE)/platform/hal/Src/stm32h5xx_hal_dma.c \
	$(SPE)/platform/hal/Src/stm32h5xx_hal_dma_ex.c \
	$(SPE)/platform/hal/Src/stm32h5xx_hal_gpio.c \
	$(SPE)/platform/hal/Src/stm32h5xx_hal_pwr.c \
	$(SPE)/platform/hal/Src/stm32h5xx_hal_pwr_ex.c \
	$(SPE)/platform/hal/Src/stm32h5xx_hal_rcc.c \
	$(SPE)/platform/hal/Src/stm32h5xx_hal_rcc_ex.c \
	$(SPE)/platform/hal/Src/stm32h5xx_hal_uart.c \
	$(SPE)/platform/hal/Src/stm32h5xx_hal_uart_ex.c

SRCS        := $(APP_SRCS) $(SPE_SRCS)
OBJS        := $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.c=.o)))
VENEERS     := $(SPE)/interface/lib/s_veneers.o
LDSCRIPT_IN := $(SPE)/platform/linker_scripts/appli_ns.ld
LDSCRIPT    := $(OUT)/appli_ns.pp.ld

.PHONY: all clean

all: $(OUT)/tfm_ns_signed.bin

$(OBJDIR) $(OUT):
	mkdir -p $@

$(LDSCRIPT): $(LDSCRIPT_IN) | $(OUT)
	@echo "PP  $(notdir $<)"
	$(CC) $(CFLAGS) -E -P -xc $< -o $@

define COMPILE_C
$(OBJDIR)/$(notdir $(1:.c=.o)): $(1) | $(OBJDIR)
	@echo "CC  $$(notdir $$<)"
	$(CC) $(CFLAGS) -c $$< -o $$@
endef
$(foreach src,$(SRCS),$(eval $(call COMPILE_C,$(src))))

$(OUT)/tfm_ns.axf: $(OBJS) $(VENEERS) $(LDSCRIPT) | $(OUT)
	@echo "LD  $(notdir $@)"
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(VENEERS)
	$(NM) $@ | grep ' tfm_psa_call_veneer$$' || true

$(OUT)/tfm_ns.bin: $(OUT)/tfm_ns.axf
	$(OBJCOPY) -O binary $< $@

$(OUT)/tfm_ns.hex: $(OUT)/tfm_ns.axf
	$(OBJCOPY) -O ihex $< $@

.PHONY: python-deps
python-deps:
	python3 -m venv $(NS_APP)/.sign-venv
	$(NS_APP)/.sign-venv/bin/python -m pip install intelhex click cryptography cbor2 pyyaml

$(OUT)/tfm_ns_signed.bin: $(OUT)/tfm_ns.bin
	@echo "SIGN $(notdir $@)"
	$(NS_APP)/sign_kit/sign.sh $(OUT)/tfm_ns.bin

$(VENEERS):
	$(error SPE export not found: $@  (set SPE=/path/to/build_s/api_ns))

clean:
	rm -rf $(OUT)
