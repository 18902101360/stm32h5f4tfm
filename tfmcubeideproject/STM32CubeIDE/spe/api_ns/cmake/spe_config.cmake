#-------------------------------------------------------------------------------
# SPDX-FileCopyrightText: Copyright The TrustedFirmware-M Contributors
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

# This CMake script template contains the set of options settled on secure side
# build but necessary for building the non-secure side too.


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was spe_config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

# TF-M Profile
set(TFM_PROFILE                                CACHE STRING "The TF-M profile")

set(TFM_PARTITION_INTERNAL_TRUSTED_STORAGE ON CACHE BOOL "Enable Internal Trusted Storage partition")
set(TFM_PARTITION_CRYPTO                   ON                   CACHE BOOL "Enable Crypto partition")
set(TFM_PARTITION_INITIAL_ATTESTATION      ON      CACHE BOOL "Enable Initial Attestation partition")
set(TFM_PARTITION_PROTECTED_STORAGE        ON        CACHE BOOL "Enable Protected Storage partition")
set(TFM_PARTITION_PLATFORM                 ON                 CACHE BOOL "Enable Platform partition")
set(TFM_PARTITION_FIRMWARE_UPDATE          ON          CACHE BOOL "Enable firmware update partition")
set(TFM_PARTITION_NS_AGENT_MAILBOX         OFF         CACHE BOOL "Enable the Mailbox agents")

# The options necessary for signing the final image
set(BL2                                    ON)
set(BL2_HEADER_SIZE                        0x400)
set(BL2_TRAILER_SIZE                       0x2000)
set(MCUBOOT_IMAGE_NUMBER                   2)
set(TFM_S_KEY_ID                           2147451243)
set(TFM_NS_KEY_ID                          2147451244)
set(MCUBOOT_CONFIRM_IMAGE                  OFF)
set(MCUBOOT_ENC_IMAGES                     OFF)
set(MCUBOOT_ENC_KEY_LEN                    128)
set_and_check(MCUBOOT_KEY_ENC_NS           ${PACKAGE_PREFIX_DIR}/)
set_and_check(MCUBOOT_KEY_ENC              ${PACKAGE_PREFIX_DIR}/)
set(MCUBOOT_MEASURED_BOOT                  ON)
set(MCUBOOT_ALIGN_VAL                      16)
set(MCUBOOT_UPGRADE_STRATEGY               OVERWRITE_ONLY)
set(MCUBOOT_S_IMAGE_MIN_VER                0.0.0+0)

set(MCUBOOT_MEASURED_BOOT                  ON)
set(MCUBOOT_HW_KEY                         ON)
set(MCUBOOT_BUILTIN_KEY                    OFF)
set(MCUBOOT_IMAGE_MULTI_SIG_SUPPORT        OFF)

set(MCUBOOT_SECURITY_COUNTER_S             1)
set(MCUBOOT_IMAGE_VERSION_S                2.3.0)
set_and_check(MCUBOOT_KEY_S                ${PACKAGE_PREFIX_DIR}/image_signing/keys/image_s_signing_private_key.pem)

set(MCUBOOT_SECURITY_COUNTER_NS            1)
set(MCUBOOT_IMAGE_VERSION_NS               0.0.0)
set_and_check(MCUBOOT_KEY_NS               ${PACKAGE_PREFIX_DIR}/image_signing/keys/image_ns_signing_private_key.pem)
set(PLATFORM_DEFAULT_IMAGE_SIGNING         ON)

set(TFM_LOAD_NS_IMAGE                      ON              CACHE BOOL   "Whether to load an NS image")

# The common options describing a platform configuration

set(TFM_PLATFORM                           stm/stm32h573i_dk                   CACHE STRING "Platform to build TF-M for. Must be either a relative path from [TF-M]/platform/ext/target, or an absolute path.")
set(CONFIG_TFM_USE_TRUSTZONE               ON       CACHE BOOL   "Use TrustZone")
set(CONFIG_TFM_SPM_BACKEND                 SFN         CACHE STRING "The SPM backend")
set(TFM_MULTI_CORE_TOPOLOGY                        CACHE BOOL   "Platform has multi core")
set(PSA_FRAMEWORK_HAS_MM_IOVEC             OFF     CACHE BOOL   "Enable the MM-IOVEC feature")
set(TFM_ISOLATION_LEVEL                    1            CACHE STRING "The TFM isolation level")

set(PLATFORM_DEFAULT_CRYPTO_KEYS           ON   CACHE BOOL   "Use the default crypto keys")
set(PLATFORM_DEFAULT_UART_STDOUT           ON   CACHE BOOL   "Use default uart stdout implementation.")

set(TFM_HYBRID_PLATFORM_API_BROKER         OFF CACHE BOOL   "Enable API broker for Hybrid Platforms")

# Other common options

# Coprocessor settings
# It is difficult to sort out coprocessor settings and their dependencies.
# Export all the essential settings and therefore NS users don't have to figure them out again or
# include other config files.
# Also export other coprocessor settings to enable NS integration to validate the whole settings
# and toolchain compatibility via installed cp_config_check.cmake.
set(CONFIG_TFM_ENABLE_FP                   ON       CACHE BOOL   "Enable/disable FP usage")
set(CONFIG_TFM_ENABLE_MVE                  OFF      CACHE BOOL   "Enable/disable integer MVE usage")
set(CONFIG_TFM_ENABLE_MVE_FP               OFF   CACHE BOOL   "Enable/disable floating-point MVE usage")
set(CONFIG_TFM_FLOAT_ABI                   hard)
set(CONFIG_TFM_DISABLE_CP10CP11            OFF CACHE BOOL  "This disables the coprocessors CP10-CP11")
set(CONFIG_TFM_ENABLE_CP10CP11             ON CACHE BOOL   "Make FPU and MVE operational when SPE and/or NSPE require FPU or MVE usage. This alone only enables the coprocessors CP10-CP11, whereas CONFIG_TFM_FLOAT_ABI=hard along with  CONFIG_TFM_ENABLE_FP, CONFIG_TFM_ENABLE_MVE or CONFIG_TFM_ENABLE_MVE_FP compiles the code with hardware FP or MVE instructions and ABI.")
set(CONFIG_TFM_LAZY_STACKING               ON   CACHE BOOL   "Enable/disable lazy stacking")

set(TFM_VERSION                            2.3.1)
set(TFM_NS_MANAGE_NSID                     OFF)

set(RECOMMENDED_TFM_TESTS_VERSION          TF-Mv2.3.1)
set(CHECK_TFM_TESTS_VERSION OFF)

set(TFM_MERGE_HEX_FILES                    OFF                 CACHE BOOL   "Create merged hex file in the end of the build")
set_and_check(TFM_S_HEX_FILE_PATH          ${PACKAGE_PREFIX_DIR}/ CACHE STRING "Merged secure hex file's path")
