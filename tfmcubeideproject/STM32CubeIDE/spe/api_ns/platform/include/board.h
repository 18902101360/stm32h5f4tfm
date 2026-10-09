/**
  ******************************************************************************
  * @file    board.h
  * @author  MCD Application Team
  * @brief   board header file for stm32h573_dk.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
#ifndef __BOARD_H__
#define __BOARD_H__

#include <stdint.h>

/* config for usart */

#if 0
#define COM_INSTANCE                           USART3
#define COM_CLK_ENABLE()                       __HAL_RCC_USART3_CLK_ENABLE()
#define COM_CLK_DISABLE()                      __HAL_RCC_USART3_CLK_DISABLE()
#define COM_TX_GPIO_PORT                       GPIOD
#define COM_TX_GPIO_CLK_ENABLE()               __HAL_RCC_GPIOD_CLK_ENABLE()
#define COM_TX_PIN                             GPIO_PIN_8
#define COM_TX_AF                              GPIO_AF7_USART3

#define COM_RX_GPIO_PORT                       GPIOD
#define COM_RX_GPIO_CLK_ENABLE()               __HAL_RCC_GPIOD_CLK_ENABLE()
#define COM_RX_PIN                             GPIO_PIN_9
#define COM_RX_AF                              GPIO_AF7_USART3
#else
#define COM_INSTANCE                           USART1
#define COM_CLK_ENABLE()                       __HAL_RCC_USART1_CLK_ENABLE()
#define COM_CLK_DISABLE()                      __HAL_RCC_USART1_CLK_DISABLE()
#define COM_TX_GPIO_PORT                       GPIOA
#define COM_TX_GPIO_CLK_ENABLE()               __HAL_RCC_GPIOA_CLK_ENABLE()
#define COM_TX_PIN                             GPIO_PIN_9
#define COM_TX_AF                              GPIO_AF7_USART1

#define COM_RX_GPIO_PORT                       GPIOA
#define COM_RX_GPIO_CLK_ENABLE()               __HAL_RCC_GPIOA_CLK_ENABLE()
#define COM_RX_PIN                             GPIO_PIN_10
#define COM_RX_AF                              GPIO_AF7_USART1

#endif
/* config for flash driver */
#define FLASH0_SECTOR_SIZE	0x2000
#define FLASH0_PAGE_SIZE 0x2000
#define FLASH0_PROG_UNIT 0x10
#define FLASH0_ERASED_VAL 0xff

/*
 * External W25Q32 GPIO bit-bang profiles.
 * BL2/NS try each entry, read JEDEC ID (ef:40:16). The first hit is kept;
 * other profiles' pins are restored to analog reset. If none hit, all
 * profile pins are restored and boot continues on internal flash.
 *
 * Do not list USART1 (PA9/PA10) or SWD (PA13/PA14).
 * Add another board by appending a w25_gpio_cfg_t (see w25_gpio_profiles[]
 * in low_level_spi_flash.c).
 */
#define W25_GPIO_PORT_A                        0U
#define W25_GPIO_PORT_B                        1U
#define W25_GPIO_PORT_C                        2U
#define W25_GPIO_PORT_D                        3U
#define W25_GPIO_PORT_E                        4U
#define W25_GPIO_PORT_F                        5U
#define W25_GPIO_PORT_G                        6U
#define W25_GPIO_PORT_H                        7U
#define W25_GPIO_PORT_I                        8U

typedef struct {
    uint8_t  port; /* W25_GPIO_PORT_* */
    uint16_t pin;  /* GPIO_PIN_* */
} w25_gpio_pad_t;

typedef struct {
    const char *name;
    w25_gpio_pad_t sck;
    w25_gpio_pad_t miso;
    w25_gpio_pad_t mosi;
    w25_gpio_pad_t cs;
} w25_gpio_cfg_t;

/* Default / first-board pads (SPI1 positions, bit-bang, AF unused). */
#define SPI1_FLASH_SCK_PORT                    GPIOA
#define SPI1_FLASH_SCK_PIN                     GPIO_PIN_5
#define SPI1_FLASH_SCK_AF                      GPIO_AF5_SPI1
#define SPI1_FLASH_MISO_PORT                   GPIOA
#define SPI1_FLASH_MISO_PIN                    GPIO_PIN_6
#define SPI1_FLASH_MISO_AF                     GPIO_AF5_SPI1
#define SPI1_FLASH_MOSI_PORT                   GPIOA
#define SPI1_FLASH_MOSI_PIN                    GPIO_PIN_7
#define SPI1_FLASH_MOSI_AF                     GPIO_AF5_SPI1
#define SPI1_FLASH_CS_PORT                     GPIOB
#define SPI1_FLASH_CS_PIN                      GPIO_PIN_2
#endif /* __BOARD_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
