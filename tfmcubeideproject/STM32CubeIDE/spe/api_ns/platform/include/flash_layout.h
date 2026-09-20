/*
 * Copyright (c) 2018-2024 Arm Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __FLASH_LAYOUT_H__
#define __FLASH_LAYOUT_H__

/* This header file is included from linker scatter file as well, where only a
 * limited C constructs are allowed. Therefore it is not possible to include
 * here the platform_retarget.h to access flash related defines. To resolve this
 * some of the values are redefined here with different names, these are marked
 * with comment.
 */
 /* Flash layout for stm32h5f4 with BL2 (multiple image boot).
  * Hardware flash is 4 MB dual-bank (2 MB per bank, 8 KB sectors).
  * NS application slot is 1024 KB; leftover flash is NS user data.
  *
  * A MCUBoot slot must not cross a bank boundary. After the S primary
  * and 1024 KB NS primary, the next byte is still in bank 1.
  * Placing S secondary there would span into bank 2 at 0x200000
  * and BL2 hangs when it first reads that slot (image 0, after NS).
  * S secondary therefore starts at bank 2; the rest of bank 1 is unused.
  *
  * S slots are 512 KB so TEST_S (about 339 KB payload today) has room
  * for later SPE growth. NS is 1024 KB (CubeIDE NS is far smaller).
  * Bank1 after ITS is 1568 KB: 512 + 1024 + 32 unused.
  *
 * 0x0000_0000 SCRATCH (48 KB)
 * 0x0000_C000 BL2 - counters(16 KB)
 * 0x0001_0000 BL2 - MCUBoot (96 KB)
 * 0x0002_8000 OTP Write Protect (16 KB)
 * 0x0002_C000 NV counters area (16 KB)
 * 0x0003_0000 Secure Storage Area (256 KB)
 * 0x0007_0000 Internal Trusted Storage Area (32 KB)
 * 0x0007_8000 Secure image     primary slot (512 KB)
 * 0x000F_8000 Non-secure image primary slot (1024 KB)
 * 0x001F_8000 unused (32 KB, remainder of bank 1)
 * 0x0020_0000 Secure image     secondary slot (512 KB, bank 2)
 * 0x0028_0000 Non-secure image secondary slot (1024 KB)
 * 0x0038_0000 Non-secure user flash data (512 KB, to end of 4 MB)
 *
 * Bl2 binary is written at 0x1_0000:
 * it contains bl2_counter init value, OTP write protect, NV counters area init.
 */

/* Flash layout info for BL2 bootloader */
#define FLASH_AREA_IMAGE_SECTOR_SIZE    (0x2000)     /* 8 KB */
#define FLASH_AREA_WRP_GROUP_SIZE       (0x8000)     /* 32 KB */
#define FLASH_B_SIZE                    (0x200000)   /* 2 MBytes per bank */
#define FLASH_TOTAL_SIZE                (FLASH_B_SIZE+FLASH_B_SIZE) /* 4 MBytes */
#define FLASH_BASE_ADDRESS              (0x0c000000) /* same as FLASH0_BASE_S */

/* Flash device ID */


/* Offset and size definitions of the flash partitions that are handled by the
 * bootloader. The image swapping is done between IMAGE_0 and IMAGE_1, SCRATCH
 * is used as a temporary storage during image swapping.
 */

/* scratch area */
#define FLASH_AREA_SCRATCH_OFFSET       (0x0)
#define FLASH_AREA_SCRATCH_SIZE         (0xC000) /* 48 KB */

/* control scratch area */
#if (FLASH_AREA_SCRATCH_OFFSET % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_AREA_SCRATCH_OFFSET not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif /* (FLASH_AREA_SCRATCH_OFFSET % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0*/

/* area for bl2 anti roll back counter */
#define FLASH_BL2_NVCNT_AREA_OFFSET     (FLASH_AREA_SCRATCH_SIZE)
#define FLASH_BL2_NVCNT_AREA_SIZE       (FLASH_AREA_IMAGE_SECTOR_SIZE+FLASH_AREA_IMAGE_SECTOR_SIZE)
/* Area for downloading bl2 image */
//#define FLASH_AREA_BL2_BIN_OFFSET         (FLASH_AREA_PERSO_OFFSET)
#define FLASH_AREA_BL2_BIN_OFFSET       (FLASH_BL2_NVCNT_AREA_OFFSET +FLASH_AREA_IMAGE_SECTOR_SIZE)
/* personal Area Not used */
#define FLASH_AREA_PERSO_OFFSET         (FLASH_BL2_NVCNT_AREA_OFFSET +FLASH_BL2_NVCNT_AREA_SIZE)
#define FLASH_AREA_PERSO_SIZE           (0x0)
/* control personal area */
#if (FLASH_AREA_PERSO_OFFSET % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_AREA_PERSO_OFFSET not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif /* FLASH_AREA_PERSO_OFFSET % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0 */

/* area for BL2 code protected by hdp */
#define FLASH_AREA_BL2_OFFSET           (FLASH_AREA_PERSO_OFFSET+FLASH_AREA_PERSO_SIZE )
#define FLASH_AREA_BL2_SIZE             (0x18000)
/* HDP area end at this address */
#define FLASH_BL2_HDP_END               (FLASH_AREA_BL2_OFFSET+FLASH_AREA_BL2_SIZE-1)
/* control area for BL2 code protected by hdp */
#if ((FLASH_AREA_BL2_OFFSET+FLASH_AREA_BL2_SIZE) % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "HDP area must be aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif /* ((FLASH_AREA_BL2_OFFSET+FLASH_AREA_BL2_SIZE) % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0 */

/* control area under WRP group protection */
#if (FLASH_AREA_BL2_OFFSET % FLASH_AREA_WRP_GROUP_SIZE) != 0
#error "FLASH_AREA_BL2_OFFSET not aligned on FLASH_AREA_WRP_GROUP_SIZE"
#endif /* (FLASH_AREA_BL2_OFFSET % FLASH_AREA_WRP_GROUP_SIZE) != 0 */
#if ((FLASH_AREA_BL2_OFFSET+FLASH_AREA_BL2_SIZE) % FLASH_AREA_WRP_GROUP_SIZE) != 0
#error "(FLASH_AREA_BL2_OFFSET+FLASH_AREA_BL2_SIZE) not aligned on FLASH_AREA_WRP_GROUP_SIZE"
#endif /* ((FLASH_AREA_BL2_OFFSET+FLASH_AREA_BL2_SIZE) % FLASH_AREA_WRP_GROUP_SIZE) != 0 */

/* area for BL2 code not protected by hdp */
#define FLASH_AREA_BL2_NOHDP_OFFSET     (FLASH_AREA_BL2_OFFSET+FLASH_AREA_BL2_SIZE)
#define FLASH_AREA_BL2_NOHDP_SIZE       (0x0)

/* OTP / Non Volatile Counters definitions */
#define FLASH_OTP_NV_COUNTERS_SECTOR_SIZE   (FLASH_AREA_IMAGE_SECTOR_SIZE)
#define FLASH_OTP_NV_COUNTERS_AREA_OFFSET         (FLASH_AREA_BL2_NOHDP_OFFSET + \
                                                   FLASH_AREA_BL2_NOHDP_SIZE)
#define FLASH_OTP_NV_COUNTERS_AREA_SIZE     (FLASH_OTP_NV_COUNTERS_SECTOR_SIZE + \
                                             FLASH_OTP_NV_COUNTERS_SECTOR_SIZE)
/* control area for BL2 code protected by hdp */
#if (FLASH_AREA_BL2_NOHDP_OFFSET % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "HDP area must be aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif /* (FLASH_AREA_BL2_NOHDP_OFFSET % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0 */

/* Non Volatile Counters definitions */
#define FLASH_NV_COUNTERS_AREA_SIZE      (FLASH_AREA_IMAGE_SECTOR_SIZE+FLASH_AREA_IMAGE_SECTOR_SIZE)
#define FLASH_NV_COUNTERS_AREA_OFFSET   (FLASH_OTP_NV_COUNTERS_AREA_OFFSET+FLASH_OTP_NV_COUNTERS_AREA_SIZE)
/* Control Non Volatile Counters definitions */
#if (FLASH_NV_COUNTER_AREA_SIZE % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_NV_COUNTER_AREA_SIZE not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif /*  (FLASH_NV_COUNTER_AREA_SIZE % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0 */

/* Secure Storage (PS) Service definitions */
#define FLASH_PS_AREA_SIZE             (32 * FLASH_AREA_IMAGE_SECTOR_SIZE) /* 256 KB */
#define FLASH_PS_AREA_OFFSET           (FLASH_NV_COUNTERS_AREA_OFFSET+FLASH_NV_COUNTERS_AREA_SIZE)

/* Control Secure Storage (PS) Service definitions*/
#if (FLASH_PS_AREA_OFFSET % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_PS_AREA_OFFSET not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif /*  (FLASH_PS_AREA_OFFSET % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0 */

/* Internal Trusted Storage (ITS) Service definitions */
#define FLASH_ITS_AREA_OFFSET           (FLASH_PS_AREA_OFFSET+FLASH_PS_AREA_SIZE)
#define FLASH_ITS_AREA_SIZE             (4 * FLASH_AREA_IMAGE_SECTOR_SIZE) /* 32 KB */

/*Control  Internal Trusted Storage (ITS) Service definitions */
#if (FLASH_ITS_AREA_OFFSET % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_ITS_AREA_OFFSET not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif /*  (FLASH_ITS_AREA_OFFSET % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0 */

#define FLASH_S_PARTITION_SIZE          (0x80000)   /* 512 KB for S partition */
#define FLASH_NS_PARTITION_SIZE         (0x100000)  /* 1024 KB for NS partition */

#define FLASH_PARTITION_SIZE            (FLASH_S_PARTITION_SIZE+FLASH_NS_PARTITION_SIZE)

#if (FLASH_S_PARTITION_SIZE > FLASH_NS_PARTITION_SIZE)
#define FLASH_MAX_PARTITION_SIZE FLASH_S_PARTITION_SIZE
#else
#define FLASH_MAX_PARTITION_SIZE FLASH_NS_PARTITION_SIZE
#endif
/* BL2 flash areas */
#define FLASH_AREA_BEGIN_OFFSET         (FLASH_ITS_AREA_OFFSET + FLASH_ITS_AREA_SIZE)

/* Secure image primary slot */
#define FLASH_AREA_0_ID                 (1)
#define FLASH_AREA_0_DEVICE_ID          (0)
#define FLASH_AREA_0_OFFSET             (FLASH_AREA_BEGIN_OFFSET)
/* Control  Secure image primary slot */
#if (FLASH_AREA_0_OFFSET  % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_AREA_0_OFFSET  not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif /*  (FLASH_AREA_0_OFFSET  % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0 */
#define FLASH_AREA_0_SIZE               (FLASH_S_PARTITION_SIZE)

/* Non-secure image primary slot */
#define FLASH_AREA_1_ID                 (FLASH_AREA_0_ID + 1)
#define FLASH_AREA_1_DEVICE_ID          (FLASH_AREA_0_DEVICE_ID)
#define FLASH_AREA_1_OFFSET             (FLASH_AREA_0_OFFSET + FLASH_AREA_0_SIZE)
/* Control Non-secure image primary slot */
#if (FLASH_AREA_1_OFFSET  % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_AREA_1_OFFSET  not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif /* (FLASH_AREA_1_OFFSET  % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0  */
#define FLASH_AREA_1_SIZE               (FLASH_NS_PARTITION_SIZE)

/* Secure image secondary slot: start of bank 2 so the slot stays in one bank. */
#define FLASH_AREA_2_ID                 (FLASH_AREA_1_ID + 1)
#define FLASH_AREA_2_DEVICE_ID          (FLASH_AREA_1_DEVICE_ID)
#define FLASH_AREA_2_OFFSET             (FLASH_B_SIZE)
/* Control  Secure image secondary slot */
#if (FLASH_AREA_2_OFFSET  % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_AREA_2_OFFSET  not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif /*   (FLASH_AREA_2_OFFSET  % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0 */
#if FLASH_AREA_2_OFFSET < (FLASH_AREA_1_OFFSET + FLASH_AREA_1_SIZE)
#error "FLASH_AREA_2 overlaps FLASH_AREA_1"
#endif
#define FLASH_AREA_2_SIZE               (FLASH_S_PARTITION_SIZE)

/* Non-secure image secondary slot */
#define FLASH_AREA_3_ID                 (FLASH_AREA_2_ID + 1)
#define FLASH_AREA_3_DEVICE_ID          (FLASH_AREA_2_DEVICE_ID)
#define FLASH_AREA_3_OFFSET             (FLASH_AREA_2_OFFSET + FLASH_AREA_2_SIZE)
#if (FLASH_AREA_3_OFFSET  % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_AREA_3_OFFSET  not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif /*  (FLASH_AREA_3_OFFSET  % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0 */
/*Control Non-secure image secondary slot */
#define FLASH_AREA_3_SIZE               (FLASH_NS_PARTITION_SIZE)
#define FLASH_AREA_END_OFFSET           (FLASH_AREA_3_OFFSET + FLASH_AREA_3_SIZE)
#define FLASH_AREA_SCRATCH_ID           (FLASH_AREA_3_ID + 1)
#define FLASH_AREA_SCRATCH_DEVICE_ID    (FLASH_AREA_3_DEVICE_ID)

/* STM32 dual-bank flash cannot host one MCUBoot slot across both banks. */
#if ((FLASH_AREA_0_OFFSET) / FLASH_B_SIZE) != \
    ((FLASH_AREA_0_OFFSET + FLASH_AREA_0_SIZE - 1) / FLASH_B_SIZE)
#error "FLASH_AREA_0 spans flash banks"
#endif
#if ((FLASH_AREA_1_OFFSET) / FLASH_B_SIZE) != \
    ((FLASH_AREA_1_OFFSET + FLASH_AREA_1_SIZE - 1) / FLASH_B_SIZE)
#error "FLASH_AREA_1 spans flash banks"
#endif
#if ((FLASH_AREA_2_OFFSET) / FLASH_B_SIZE) != \
    ((FLASH_AREA_2_OFFSET + FLASH_AREA_2_SIZE - 1) / FLASH_B_SIZE)
#error "FLASH_AREA_2 spans flash banks"
#endif
#if ((FLASH_AREA_3_OFFSET) / FLASH_B_SIZE) != \
    ((FLASH_AREA_3_OFFSET + FLASH_AREA_3_SIZE - 1) / FLASH_B_SIZE)
#error "FLASH_AREA_3 spans flash banks"
#endif

/* Remaining flash after MCUBoot slots: NS-side user data (not an image slot). */
#define FLASH_NS_USER_DATA_OFFSET       (FLASH_AREA_END_OFFSET)
#define FLASH_NS_USER_DATA_SIZE         (FLASH_TOTAL_SIZE - FLASH_NS_USER_DATA_OFFSET)
#if (FLASH_NS_USER_DATA_OFFSET % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_NS_USER_DATA_OFFSET not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif
#if (FLASH_NS_USER_DATA_SIZE % FLASH_AREA_IMAGE_SECTOR_SIZE) != 0
#error "FLASH_NS_USER_DATA_SIZE not aligned on FLASH_AREA_IMAGE_SECTOR_SIZE"
#endif
#if (FLASH_NS_USER_DATA_SIZE == 0)
#error "No flash left for NS user data"
#endif


/*
 * The maximum number of status entries supported by the bootloader.
 */
#define MCUBOOT_STATUS_MAX_ENTRIES         ((FLASH_MAX_PARTITION_SIZE) / \
                                            FLASH_AREA_SCRATCH_SIZE)
/* Maximum number of image sectors supported by the bootloader.
 * NS is 1024 KB / 8 KB = 128. MCUBoot sizes the swap-status trailer for this
 * count in every slot, including the smaller S slot. A matching MCUBoot patch
 * (0002) drops an erased swap_size so a factory S image is not treated as a
 * mid-swap and does not walk the sector table off the end of SRAM.
 */
#define MCUBOOT_MAX_IMG_SECTORS           ((FLASH_MAX_PARTITION_SIZE) / \
                                           FLASH_AREA_IMAGE_SECTOR_SIZE)

#define SECURE_IMAGE_OFFSET             (0x0)
#define SECURE_IMAGE_MAX_SIZE           FLASH_S_PARTITION_SIZE

#define NON_SECURE_IMAGE_OFFSET         (SECURE_IMAGE_OFFSET + SECURE_IMAGE_MAX_SIZE)
#define NON_SECURE_IMAGE_MAX_SIZE       FLASH_NS_PARTITION_SIZE

/* Flash device name used by BL2 and NV Counter
 * Name is defined in flash driver file: low_level_flash.c
 */
#define TFM_NV_COUNTERS_FLASH_DEV       TFM_Driver_FLASH0
#define FLASH_DEV_NAME                  TFM_Driver_FLASH0
#define TFM_HAL_FLASH_PROGRAM_UNIT       (0x10)
/* Protected Storage (PS) Service definitions
 * Note: Further documentation of these definitions can be found in the
 * TF-M PS Integration Guide.
 */
#define TFM_HAL_PS_FLASH_DRIVER TFM_Driver_FLASH0

/* In this target the CMSIS driver requires only the offset from the base
 * address instead of the full memory address.
 */
#define PS_SECTOR_SIZE                 FLASH_AREA_IMAGE_SECTOR_SIZE
/* The sectors must be in consecutive memory location */
#define PS_NBR_OF_SECTORS              (FLASH_PS_AREA_SIZE / PS_SECTOR_SIZE)
/* The maximum asset size to be stored in the ITS area */
#define ITS_SECTOR_SIZE                 FLASH_AREA_IMAGE_SECTOR_SIZE
/* The sectors must be in consecutive memory location */
#define ITS_NBR_OF_SECTORS              (FLASH_ITS_AREA_SIZE / ITS_SECTOR_SIZE)

/* Base address of dedicated flash area for PS */
#define TFM_HAL_PS_FLASH_AREA_ADDR    FLASH_PS_AREA_OFFSET
/* Size of dedicated flash area for PS */
#define TFM_HAL_PS_FLASH_AREA_SIZE    FLASH_PS_AREA_SIZE
#define PS_RAM_FS_SIZE                TFM_HAL_PS_FLASH_AREA_SIZE
/* Number of physical erase sectors per logical FS block */
#define TFM_HAL_PS_SECTORS_PER_BLOCK  (1)
/* Smallest flash programmable unit in bytes */
#define TFM_HAL_PS_PROGRAM_UNIT       (0x10)

/* Internal Trusted Storage (ITS) Service definitions
 * Note: Further documentation of these definitions can be found in the
 * TF-M ITS Integration Guide.
 */
#define TFM_HAL_ITS_FLASH_DRIVER TFM_Driver_FLASH0

/* In this target the CMSIS driver requires only the offset from the base
 * address instead of the full memory address.
 */
/* Base address of dedicated flash area for ITS */
#define TFM_HAL_ITS_FLASH_AREA_ADDR    FLASH_ITS_AREA_OFFSET
/* Size of dedicated flash area for ITS */
#define TFM_HAL_ITS_FLASH_AREA_SIZE    FLASH_ITS_AREA_SIZE
#define ITS_RAM_FS_SIZE                TFM_HAL_ITS_FLASH_AREA_SIZE
/* Number of physical erase sectors per logical FS block */
#define TFM_HAL_ITS_SECTORS_PER_BLOCK  (1)
/* Smallest flash programmable unit in bytes */
#define TFM_HAL_ITS_PROGRAM_UNIT       (0x10)

/* OTP / NV counter definitions */
#define TFM_OTP_NV_COUNTERS_AREA_SIZE   (FLASH_OTP_NV_COUNTERS_AREA_SIZE / 2)
#define TFM_OTP_NV_COUNTERS_AREA_ADDR   FLASH_OTP_NV_COUNTERS_AREA_OFFSET
#define TFM_OTP_NV_COUNTERS_SECTOR_SIZE FLASH_OTP_NV_COUNTERS_SECTOR_SIZE
#define TFM_OTP_NV_COUNTERS_BACKUP_AREA_ADDR (TFM_OTP_NV_COUNTERS_AREA_ADDR + \
                                              TFM_OTP_NV_COUNTERS_AREA_SIZE)
/* NV Counters definitions */
#define TFM_NV_COUNTERS_AREA_ADDR        FLASH_NV_COUNTERS_AREA_OFFSET
#define TFM_NV_COUNTERS_AREA_SIZE        (0x20)/* 32 Bytes */
#define TFM_NV_COUNTERS_SECTOR_ADDR      FLASH_NV_COUNTERS_AREA_OFFSET
#define TFM_NV_COUNTERS_SECTOR_SIZE      FLASH_AREA_IMAGE_SECTOR_SIZE

/* BL2 NV Counters definitions  */
#define BL2_NV_COUNTERS_AREA_ADDR        FLASH_BL2_NVCNT_AREA_OFFSET
#define BL2_NV_COUNTERS_AREA_SIZE        FLASH_BL2_NVCNT_AREA_SIZE

/* FIXME: not valid today */
#define BL2_S_RAM_ALIAS_BASE             (0x30000000)
#define BL2_NS_RAM_ALIAS_BASE            (0x20000000)

/*  This area in SRAM 2 is updated BL2 and can be lock to avoid any changes */
#define BOOT_TFM_SHARED_DATA_SIZE        (0x400)
#define BOOT_TFM_SHARED_DATA_BASE        (0x3005fc00) /* end of SRAM2 (128 KB) */
#define SHARED_BOOT_MEASUREMENT_BASE     BOOT_TFM_SHARED_DATA_BASE
#define SHARED_BOOT_MEASUREMENT_SIZE     BOOT_TFM_SHARED_DATA_SIZE

/* OBK */
#define OBK_HDPL0_OFFSET        (0x00U)         /* First OBkey Hdpl 0 */
#define OBK_HDPL0_END           (0xFFU)         /* Last OBKey Hdpl 0 */
#define OBK_HDPL1_OFFSET        (0x100U)        /* First OBkey Hdpl 1 */
#define OBK_HDPL1_END           (0x8FFU)        /* Last OBKey Hdpl 1 */
#define OBK_HDPL2_OFFSET        (0x900U)        /* First OBkey Hdpl 2 */
#define OBK_HDPL2_END           (0xBFFU)        /* Last OBKey Hdpl 2 */
#define OBK_HDPL3_OFFSET        (0xC00U)        /* First OBkey Hdpl 3 */
#define OBK_HDPL3_END           (0x1FFFU)       /* Last OBKey Hdpl 3 */

/* Engi bits */
#define ENGI_BASE_NS                        (0x08FFF800U)
#define ENGI_SIZE                           (0x40U)

/* Systeme Flash description */
#define RSS_LIB_BASE                        (0x0FF94000U)
#define RSS_LIB_SIZE                        (0x2000U)
#define BOOTLOADER_BASE_NS                  (0x0BF97000U)
#define BOOTLOADER_SIZE                     (0x9400U)
#define STM32_DESCRIPTOR_BASE_NS_3          (0x0BF9FB00U)
#define STM32_DESCRIPTOR_BASE_NS_2          (0x0BF9FD00U)
#define STM32_DESCRIPTOR_BASE_NS_1          (0x0BF9FE00U)
#define RSSLIB_PFUNC_3                      (0x0BF9FB68UL)
#define RSSLIB_PFUNC_2                      (0x0BF9FD68UL)
#define RSSLIB_PFUNC_1                      (0x0BF9FE68UL)
#define STM32_DESCRIPTOR_SIZE               (0x100U)
#define STM32_DESCRIPTOR_BASE_NS            (STM32_DESCRIPTOR_BASE_NS_3) /* use for mpu region the lowest address*/
#define STM32_DESCRIPTOR_END_NS             (STM32_DESCRIPTOR_BASE_NS_1 + STM32_DESCRIPTOR_SIZE -1) /* to cover all descriptors */

#endif /* __FLASH_LAYOUT_H__ */
