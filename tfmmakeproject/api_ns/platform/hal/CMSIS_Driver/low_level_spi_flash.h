/**
  ******************************************************************************
  * @file    low_level_spi_flash.h
  * @brief   CMSIS Flash driver for W25Q32 on STM32H573 SPI1
  ******************************************************************************
  */
#ifndef __LOW_LEVEL_SPI_FLASH_H
#define __LOW_LEVEL_SPI_FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Driver_Flash.h"

extern ARM_DRIVER_FLASH TFM_Driver_SPI_FLASH0;

/*
 * W25Q32 helpers. GPIO bit-bang; pin sets are w25_gpio_profiles[]
 * (default SPI1 pads PA5/PA6/PA7, CS PB2).
 * BL2 only uses init/read/JEDEC; after a successful overwrite it also
 * erases that download slot (w25q32_erase_range). Program remains a no-op.
 * NS erases and writes signed tfm_s_signed.bin / tfm_ns_signed.bin into
 * SPI_FLASH_S_UPDATE_OFFSET / SPI_FLASH_NS_UPDATE_OFFSET.
 */
int32_t w25q32_init(void);
int32_t w25q32_read(uint32_t addr, void *buf, uint32_t len);
int32_t w25q32_write(uint32_t addr, const void *buf, uint32_t len);
int32_t w25q32_erase_4k(uint32_t addr);
int32_t w25q32_erase_range(uint32_t addr, uint32_t len);
int32_t w25q32_read_jedec_id(uint8_t id[3]);

#ifdef __cplusplus
}
#endif

#endif /* __LOW_LEVEL_SPI_FLASH_H */
