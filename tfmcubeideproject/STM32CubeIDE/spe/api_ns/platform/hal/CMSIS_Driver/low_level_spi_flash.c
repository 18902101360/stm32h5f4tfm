/**
  ******************************************************************************
  * @file    low_level_spi_flash.c
  * @brief   W25Q32 NOR via GPIO bit-bang; JEDEC-probed pin profiles
  ******************************************************************************
  */
#include "stm32hal.h"
#include "low_level_spi_flash.h"
#include "flash_layout.h"
#include "board.h"
#include <string.h>

#ifndef ARG_UNUSED
#define ARG_UNUSED(arg)  ((void)arg)
#endif

#define ARM_SPI_FLASH_DRV_VERSION   ARM_DRIVER_VERSION_MAJOR_MINOR(1, 0)

#define W25_CMD_WRITE_ENABLE        0x06U
#define W25_CMD_READ_STATUS         0x05U
#define W25_CMD_READ_DATA           0x03U
#define W25_CMD_PAGE_PROGRAM        0x02U
#define W25_CMD_SECTOR_ERASE_4K     0x20U
#define W25_CMD_BLOCK_ERASE_64K     0xD8U
#define W25_CMD_JEDEC_ID            0x9FU
#define W25_STATUS_WIP              0x01U
#define W25_JEDEC_MANU              0xEFU
#define W25_JEDEC_TYPE              0x40U
#define W25_JEDEC_CAP               0x16U
#define W25_BLOCK_64K               0x10000U

/* 64 KB erase can take up to ~2 s; bit-bang status polls are cheap. */
#define SPI_WIP_TIMEOUT             30000000U
#define W25_CMD_RELEASE_DPD         0xABU
#define W25_CMD_ENABLE_RESET        0x66U
#define W25_CMD_RESET               0x99U

/*
 * Bit-bang SPI (mode 0). STM32H573 SPI1 kernel clock is PLL1Q only; a
 * register-level master xfer timed out (JEDEC 00:00:00) and stopped BL2.
 * GPIO does not need SPI1.
 *
 * Do not key this on BL2 / MCUBOOT: those macros are also defined when SPE
 * compiles this file into platform_s. TFM_SPI_FLASH_IN_BL2 is PRIVATE on
 * platform_bl2 only.
 */
#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U) && \
    !defined(TFM_SPI_FLASH_IN_BL2)
#define W25_MARK_PINS_NS     1
#else
#define W25_MARK_PINS_NS     0
#endif

/*
 * Board GPIO sets. First JEDEC match (W25Q32 ef:40:16) wins.
 * Append more entries for other wiring; do not reuse USART/SWD pins.
 */
static const w25_gpio_cfg_t w25_gpio_profiles[] = {
    {
        .name = "PA5/PA6/PA7 CS=PB2",
        .sck  = { W25_GPIO_PORT_A, GPIO_PIN_5 },
        .miso = { W25_GPIO_PORT_A, GPIO_PIN_6 },
        .mosi = { W25_GPIO_PORT_A, GPIO_PIN_7 },
        .cs   = { W25_GPIO_PORT_B, GPIO_PIN_2 },
    },
    /*
     * Example second board (uncomment and set pins):
     * {
     *     .name = "PBx/PBy/...",
     *     .sck  = { W25_GPIO_PORT_B, GPIO_PIN_3 },
     *     .miso = { W25_GPIO_PORT_B, GPIO_PIN_4 },
     *     .mosi = { W25_GPIO_PORT_B, GPIO_PIN_5 },
     *     .cs   = { W25_GPIO_PORT_B, GPIO_PIN_6 },
     * },
     */
};
#define W25_GPIO_PROFILE_COUNT \
    (sizeof(w25_gpio_profiles) / sizeof(w25_gpio_profiles[0]))

#ifdef TFM_SPI_FLASH_IN_BL2
#include "bootutil/bootutil_log.h"
#include "bootutil/image.h"
#include "mcuboot_config/mcuboot_config.h"
#define SPI_FLASH_LOG_INF(...) BOOT_LOG_INF(__VA_ARGS__)
#define SPI_FLASH_LOG_ERR(...) BOOT_LOG_ERR(__VA_ARGS__)
#else
#define SPI_FLASH_LOG_INF(...)
#define SPI_FLASH_LOG_ERR(...)
#endif

static const ARM_DRIVER_VERSION DriverVersion = {
    ARM_FLASH_API_VERSION,
    ARM_SPI_FLASH_DRV_VERSION
};

static const ARM_FLASH_CAPABILITIES DriverCapabilities = {
    0, /* event_ready */
    0, /* data_width 8-bit */
    0, /* erase_chip not supported */
    0  /* reserved */
};

static ARM_FLASH_INFO SPI_FLASH0_DEV_DATA = {
    .sector_info  = NULL,
    .sector_count = SPI_FLASH_TOTAL_SIZE / SPI_FLASH_SECTOR_SIZE,
    .sector_size  = SPI_FLASH_SECTOR_SIZE,
    .page_size    = SPI_FLASH_PAGE_SIZE,
    .program_unit = 1,
    .erased_value = 0xFF
};

static ARM_FLASH_STATUS SPI_FLASH0_STATUS = {0, 0, 0};
static uint8_t spi_inited;
static GPIO_TypeDef *w25_sck;
static GPIO_TypeDef *w25_miso;
static GPIO_TypeDef *w25_mosi;
static GPIO_TypeDef *w25_cs;
static uint16_t w25_sck_pin;
static uint16_t w25_miso_pin;
static uint16_t w25_mosi_pin;
static uint16_t w25_cs_pin;

/* Chip tRES1/tRST only. Bit-bang SCK has no extra delay: GPIO BSRR/IDR
 * already exceeds W25Q32 tCLQV / tCSS at ~250 MHz.
 */
static void spi_spin(uint32_t n)
{
    volatile uint32_t i = n;

    while (i > 0U) {
        i--;
    }
}

static GPIO_TypeDef *w25_port_base(uint8_t port)
{
    switch (port) {
    case W25_GPIO_PORT_A: return GPIOA;
    case W25_GPIO_PORT_B: return GPIOB;
    case W25_GPIO_PORT_C: return GPIOC;
    case W25_GPIO_PORT_D: return GPIOD;
    case W25_GPIO_PORT_E: return GPIOE;
    case W25_GPIO_PORT_F: return GPIOF;
    case W25_GPIO_PORT_G: return GPIOG;
    case W25_GPIO_PORT_H: return GPIOH;
#ifdef GPIOI
    case W25_GPIO_PORT_I: return GPIOI;
#endif
    default:              return GPIOA;
    }
}

static GPIO_TypeDef *w25_port_s(uint8_t port)
{
#if defined(GPIOA_S)
    switch (port) {
    case W25_GPIO_PORT_A: return GPIOA_S;
    case W25_GPIO_PORT_B: return GPIOB_S;
    case W25_GPIO_PORT_C: return GPIOC_S;
    case W25_GPIO_PORT_D: return GPIOD_S;
    case W25_GPIO_PORT_E: return GPIOE_S;
    case W25_GPIO_PORT_F: return GPIOF_S;
    case W25_GPIO_PORT_G: return GPIOG_S;
    case W25_GPIO_PORT_H: return GPIOH_S;
#ifdef GPIOI_S
    case W25_GPIO_PORT_I: return GPIOI_S;
#endif
    default:              return GPIOA_S;
    }
#else
    return w25_port_base(port);
#endif
}

static GPIO_TypeDef *w25_port_ns(uint8_t port)
{
#if defined(GPIOA_NS)
    switch (port) {
    case W25_GPIO_PORT_A: return GPIOA_NS;
    case W25_GPIO_PORT_B: return GPIOB_NS;
    case W25_GPIO_PORT_C: return GPIOC_NS;
    case W25_GPIO_PORT_D: return GPIOD_NS;
    case W25_GPIO_PORT_E: return GPIOE_NS;
    case W25_GPIO_PORT_F: return GPIOF_NS;
    case W25_GPIO_PORT_G: return GPIOG_NS;
    case W25_GPIO_PORT_H: return GPIOH_NS;
#ifdef GPIOI_NS
    case W25_GPIO_PORT_I: return GPIOI_NS;
#endif
    default:              return GPIOA_NS;
    }
#else
    return w25_port_base(port);
#endif
}

static GPIO_TypeDef *w25_port_data(uint8_t port)
{
#if defined(TFM_SPI_FLASH_IN_BL2)
    return w25_port_s(port);
#elif defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
    return w25_port_ns(port);
#else
    return w25_port_base(port);
#endif
}

static GPIO_TypeDef *w25_port_cfg(uint8_t port)
{
#if W25_MARK_PINS_NS
    return w25_port_s(port);
#else
    return w25_port_data(port);
#endif
}

static void w25_port_clk_enable(uint8_t port)
{
    switch (port) {
    case W25_GPIO_PORT_A: __HAL_RCC_GPIOA_CLK_ENABLE(); break;
    case W25_GPIO_PORT_B: __HAL_RCC_GPIOB_CLK_ENABLE(); break;
    case W25_GPIO_PORT_C: __HAL_RCC_GPIOC_CLK_ENABLE(); break;
    case W25_GPIO_PORT_D: __HAL_RCC_GPIOD_CLK_ENABLE(); break;
    case W25_GPIO_PORT_E: __HAL_RCC_GPIOE_CLK_ENABLE(); break;
    case W25_GPIO_PORT_F: __HAL_RCC_GPIOF_CLK_ENABLE(); break;
    case W25_GPIO_PORT_G: __HAL_RCC_GPIOG_CLK_ENABLE(); break;
    case W25_GPIO_PORT_H: __HAL_RCC_GPIOH_CLK_ENABLE(); break;
#ifdef GPIOI
    case W25_GPIO_PORT_I: __HAL_RCC_GPIOI_CLK_ENABLE(); break;
#endif
    default:
        break;
    }
}

static int w25_pad_eq(w25_gpio_pad_t a, w25_gpio_pad_t b)
{
    return (a.port == b.port) && (a.pin == b.pin);
}

static int w25_pad_in_cfg(w25_gpio_pad_t pad, const w25_gpio_cfg_t *cfg)
{
    if (cfg == NULL) {
        return 0;
    }
    return w25_pad_eq(pad, cfg->sck) || w25_pad_eq(pad, cfg->miso) ||
           w25_pad_eq(pad, cfg->mosi) || w25_pad_eq(pad, cfg->cs);
}

static void w25_restore_pad(w25_gpio_pad_t pad)
{
    GPIO_TypeDef *cfg = w25_port_cfg(pad.port);

    w25_port_clk_enable(pad.port);
    HAL_GPIO_DeInit(cfg, pad.pin);
#if defined(TFM_SPI_FLASH_IN_BL2) || W25_MARK_PINS_NS
    /* Analog + SEC is the H5 reset default for these pins. */
    HAL_GPIO_ConfigPinAttributes(w25_port_s(pad.port), pad.pin, GPIO_PIN_SEC);
#endif
}

static void w25_restore_cfg(const w25_gpio_cfg_t *cfg, const w25_gpio_cfg_t *keep)
{
    if (cfg == NULL) {
        return;
    }
    if (!w25_pad_in_cfg(cfg->sck, keep)) {
        w25_restore_pad(cfg->sck);
    }
    if (!w25_pad_in_cfg(cfg->miso, keep)) {
        w25_restore_pad(cfg->miso);
    }
    if (!w25_pad_in_cfg(cfg->mosi, keep)) {
        w25_restore_pad(cfg->mosi);
    }
    if (!w25_pad_in_cfg(cfg->cs, keep)) {
        w25_restore_pad(cfg->cs);
    }
}

static void w25_restore_unused(const w25_gpio_cfg_t *keep)
{
    uint32_t i;

    for (i = 0U; i < W25_GPIO_PROFILE_COUNT; i++) {
        w25_restore_cfg(&w25_gpio_profiles[i], keep);
    }
}

static void w25_select_cfg(const w25_gpio_cfg_t *cfg)
{
    w25_sck = w25_port_data(cfg->sck.port);
    w25_miso = w25_port_data(cfg->miso.port);
    w25_mosi = w25_port_data(cfg->mosi.port);
    w25_cs = w25_port_data(cfg->cs.port);
    w25_sck_pin = cfg->sck.pin;
    w25_miso_pin = cfg->miso.pin;
    w25_mosi_pin = cfg->mosi.pin;
    w25_cs_pin = cfg->cs.pin;
}

static void cs_low(void)
{
    w25_cs->BSRR = ((uint32_t)w25_cs_pin << 16);
}

static void cs_high(void)
{
    w25_cs->BSRR = w25_cs_pin;
}

static uint8_t spi_byte(uint8_t out)
{
    uint8_t in = 0U;
    uint32_t bit;

    for (bit = 0U; bit < 8U; bit++) {
        if ((out & 0x80U) != 0U) {
            w25_mosi->BSRR = w25_mosi_pin;
        } else {
            w25_mosi->BSRR = ((uint32_t)w25_mosi_pin << 16);
        }
        out = (uint8_t)(out << 1);
        w25_sck->BSRR = w25_sck_pin;
        in = (uint8_t)(in << 1);
        if ((w25_miso->IDR & w25_miso_pin) != 0U) {
            in |= 1U;
        }
        w25_sck->BSRR = ((uint32_t)w25_sck_pin << 16);
    }

    return in;
}

static int spi_hw_init_cfg(const w25_gpio_cfg_t *cfg)
{
    GPIO_InitTypeDef gpio = {0};
    GPIO_TypeDef *sck_cfg = w25_port_cfg(cfg->sck.port);
    GPIO_TypeDef *miso_cfg = w25_port_cfg(cfg->miso.port);
    GPIO_TypeDef *mosi_cfg = w25_port_cfg(cfg->mosi.port);
    GPIO_TypeDef *cs_cfg = w25_port_cfg(cfg->cs.port);

    w25_select_cfg(cfg);
    w25_port_clk_enable(cfg->sck.port);
    w25_port_clk_enable(cfg->miso.port);
    w25_port_clk_enable(cfg->mosi.port);
    w25_port_clk_enable(cfg->cs.port);

#if defined(TFM_SPI_FLASH_IN_BL2)
    /* Secure writes to an NSEC-attributed pin are ignored on H5. */
    HAL_GPIO_ConfigPinAttributes(w25_port_s(cfg->sck.port),
                                 cfg->sck.pin, GPIO_PIN_SEC);
    HAL_GPIO_ConfigPinAttributes(w25_port_s(cfg->miso.port),
                                 cfg->miso.pin, GPIO_PIN_SEC);
    HAL_GPIO_ConfigPinAttributes(w25_port_s(cfg->mosi.port),
                                 cfg->mosi.pin, GPIO_PIN_SEC);
    HAL_GPIO_ConfigPinAttributes(w25_port_s(cfg->cs.port),
                                 cfg->cs.pin, GPIO_PIN_SEC);
#endif

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = 0;
    gpio.Pin = cfg->sck.pin;
    HAL_GPIO_Init(sck_cfg, &gpio);
    HAL_GPIO_WritePin(sck_cfg, cfg->sck.pin, GPIO_PIN_RESET);

    gpio.Pin = cfg->mosi.pin;
    HAL_GPIO_Init(mosi_cfg, &gpio);
    HAL_GPIO_WritePin(mosi_cfg, cfg->mosi.pin, GPIO_PIN_RESET);

    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin = cfg->miso.pin;
    HAL_GPIO_Init(miso_cfg, &gpio);

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin = cfg->cs.pin;
    HAL_GPIO_Init(cs_cfg, &gpio);
    HAL_GPIO_WritePin(cs_cfg, cfg->cs.pin, GPIO_PIN_SET);

#if W25_MARK_PINS_NS
    HAL_GPIO_ConfigPinAttributes(w25_port_s(cfg->sck.port),
                                 cfg->sck.pin, GPIO_PIN_NSEC);
    HAL_GPIO_ConfigPinAttributes(w25_port_s(cfg->miso.port),
                                 cfg->miso.pin, GPIO_PIN_NSEC);
    HAL_GPIO_ConfigPinAttributes(w25_port_s(cfg->mosi.port),
                                 cfg->mosi.pin, GPIO_PIN_NSEC);
    HAL_GPIO_ConfigPinAttributes(w25_port_s(cfg->cs.port),
                                 cfg->cs.pin, GPIO_PIN_NSEC);
#endif

    return 0;
}

static int spi_xfer(const uint8_t *tx, uint8_t *rx, uint32_t len)
{
    uint32_t i;

    if (len == 0U) {
        return -1;
    }

    for (i = 0U; i < len; i++) {
        uint8_t b = spi_byte((tx != NULL) ? tx[i] : 0xFFU);

        if (rx != NULL) {
            rx[i] = b;
        }
    }

    return 0;
}

static int w25_cmd(const uint8_t *cmd, uint32_t cmd_len,
                   const uint8_t *data, uint32_t data_len,
                   uint8_t *rx, uint32_t rx_len)
{
    uint8_t buf[16];
    int rc;

    if ((cmd_len + ((data != NULL) ? data_len : 0U) + rx_len) > sizeof(buf)) {
        /* Large payload: split into cmd then data in one CS window via two xfers
         * is not valid (CS would rise). Use the streaming path instead.
         */
        return -1;
    }

    memcpy(buf, cmd, cmd_len);
    if ((data != NULL) && (data_len != 0U)) {
        memcpy(&buf[cmd_len], data, data_len);
    }

    cs_low();
    if (rx_len != 0U) {
        uint8_t all[16 + 16];
        uint32_t total = cmd_len + rx_len;

        memcpy(all, buf, cmd_len);
        memset(&all[cmd_len], 0xFF, rx_len);
        rc = spi_xfer(all, all, total);
        if ((rc == 0) && (rx != NULL)) {
            memcpy(rx, &all[cmd_len], rx_len);
        }
    } else {
        rc = spi_xfer(buf, NULL, cmd_len + data_len);
    }
    cs_high();
    return rc;
}

static int w25_wait_ready(void)
{
    uint8_t cmd = W25_CMD_READ_STATUS;
    uint8_t sr;
    uint32_t timeout = SPI_WIP_TIMEOUT;

    while (timeout-- != 0U) {
        if (w25_cmd(&cmd, 1U, NULL, 0U, &sr, 1U) != 0) {
            return -1;
        }
        if ((sr & W25_STATUS_WIP) == 0U) {
            return 0;
        }
    }
    return -1;
}

static int w25_write_enable(void)
{
    uint8_t cmd = W25_CMD_WRITE_ENABLE;

    return w25_cmd(&cmd, 1U, NULL, 0U, NULL, 0U);
}

static void w25_wakeup_reset(void)
{
    uint8_t cmd;
    uint32_t i;

    /* Idle clocks with CS high in case the NOR was left mid-command. */
    for (i = 0U; i < 16U; i++) {
        (void)spi_byte(0xFFU);
    }

    cmd = W25_CMD_RELEASE_DPD;
    (void)w25_cmd(&cmd, 1U, NULL, 0U, NULL, 0U);
    spi_spin(8000U);

    cmd = W25_CMD_ENABLE_RESET;
    (void)w25_cmd(&cmd, 1U, NULL, 0U, NULL, 0U);
    cmd = W25_CMD_RESET;
    (void)w25_cmd(&cmd, 1U, NULL, 0U, NULL, 0U);
    spi_spin(40000U);
}

static int w25_stream_read(uint32_t addr, uint8_t *data, uint32_t len)
{
    uint8_t buf[4 + 256];
    uint32_t chunk;
    uint32_t done = 0U;

    while (done < len) {
        chunk = len - done;
        if (chunk > 256U) {
            chunk = 256U;
        }

        buf[0] = W25_CMD_READ_DATA;
        buf[1] = (uint8_t)((addr + done) >> 16);
        buf[2] = (uint8_t)((addr + done) >> 8);
        buf[3] = (uint8_t)(addr + done);
        memset(&buf[4], 0xFF, chunk);

        cs_low();
        if (spi_xfer(buf, buf, 4U + chunk) != 0) {
            cs_high();
            return -1;
        }
        cs_high();
        memcpy(&data[done], &buf[4], chunk);
        done += chunk;
    }
    return 0;
}

#if defined(TFM_SPI_FLASH_IN_BL2)
/*
 * NS writes signed.bin without MCUboot trailer MAGIC. Present MAGIC to BL2
 * when the slot starts with IMAGE_MAGIC so overwrite-only can consider it.
 * Matches boot_img_magic for MCUBOOT_BOOT_MAX_ALIGN != 8 (this platform: 16).
 */
#ifndef MCUBOOT_BOOT_MAX_ALIGN
#define MCUBOOT_BOOT_MAX_ALIGN 16
#endif
#if MCUBOOT_BOOT_MAX_ALIGN == 8
static const uint8_t k_mcuboot_magic[16] = {
    0x77, 0xc2, 0x95, 0xf3, 0x60, 0xd2, 0xef, 0x7f,
    0x35, 0x52, 0x50, 0x0f, 0x2c, 0xb6, 0x79, 0x80
};
#else
static const uint8_t k_mcuboot_magic[16] = {
    (uint8_t)MCUBOOT_BOOT_MAX_ALIGN,
    (uint8_t)(MCUBOOT_BOOT_MAX_ALIGN >> 8),
    0x2d, 0xe1, 0x5d, 0x29, 0x41, 0x0b,
    0x8d, 0x77, 0x67, 0x9c, 0x11, 0x0f, 0x1f, 0x8a
};
#endif

static int slot_starts_with_image_magic(uint32_t slot_off)
{
    uint8_t b[4];
    uint32_t magic;

    if (w25_stream_read(slot_off, b, 4U) != 0) {
        return 0;
    }
    magic = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
            ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return magic == IMAGE_MAGIC;
}

static void overlay_pending_magic(uint32_t addr, uint8_t *data, uint32_t cnt)
{
    const uint32_t slots[2][2] = {
        { FLASH_AREA_2_OFFSET, FLASH_AREA_2_SIZE },
        { FLASH_AREA_3_OFFSET, FLASH_AREA_3_SIZE },
    };
    uint32_t i;
    uint32_t magic_addr;
    uint32_t rd_end;
    uint32_t mag_end;
    uint32_t from;
    uint32_t to;
    uint32_t off;

    if ((data == NULL) || (cnt == 0U)) {
        return;
    }
    if (addr > (0xFFFFFFFFU - (cnt - 1U))) {
        return;
    }
    rd_end = addr + cnt;
    for (i = 0U; i < 2U; i++) {
        if (slots[i][1] < 16U) {
            continue;
        }
        magic_addr = slots[i][0] + slots[i][1] - 16U;
        mag_end = magic_addr + 16U;
        if ((rd_end <= magic_addr) || (addr >= mag_end)) {
            continue;
        }
        if (!slot_starts_with_image_magic(slots[i][0])) {
            continue;
        }
        from = (addr > magic_addr) ? addr : magic_addr;
        to = (rd_end < mag_end) ? rd_end : mag_end;
        for (off = from; off < to; off++) {
            data[off - addr] = k_mcuboot_magic[off - magic_addr];
        }
    }
}
#endif /* TFM_SPI_FLASH_IN_BL2 */

#ifndef TFM_SPI_FLASH_IN_BL2
static int w25_page_program(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint8_t buf[4 + 256];

    if ((len == 0U) || (len > SPI_FLASH_PAGE_SIZE)) {
        return -1;
    }
    if (((addr & (SPI_FLASH_PAGE_SIZE - 1U)) + len) > SPI_FLASH_PAGE_SIZE) {
        return -1;
    }

    if (w25_write_enable() != 0) {
        return -1;
    }

    buf[0] = W25_CMD_PAGE_PROGRAM;
    buf[1] = (uint8_t)(addr >> 16);
    buf[2] = (uint8_t)(addr >> 8);
    buf[3] = (uint8_t)addr;
    memcpy(&buf[4], data, len);

    cs_low();
    if (spi_xfer(buf, NULL, 4U + len) != 0) {
        cs_high();
        return -1;
    }
    cs_high();
    return w25_wait_ready();
}
#endif /* !TFM_SPI_FLASH_IN_BL2 */

static int is_slot_range(uint32_t addr, uint32_t len)
{
    uint32_t end;
    uint32_t slot_end = FLASH_AREA_3_OFFSET + FLASH_AREA_3_SIZE - 1U;

    if (len == 0U) {
        return 0;
    }
    if (addr < FLASH_AREA_2_OFFSET) {
        return 0;
    }
    if (addr > (0xFFFFFFFFU - (len - 1U))) {
        return 0;
    }
    end = addr + len - 1U;
    if (end > slot_end) {
        return 0;
    }
    return 1;
}

static ARM_DRIVER_VERSION Flash_GetVersion(void)
{
    return DriverVersion;
}

static ARM_FLASH_CAPABILITIES Flash_GetCapabilities(void)
{
    return DriverCapabilities;
}

static int w25_jedec_match(uint8_t id[3])
{
    /* W25Q32 = ef:40:16; 00:00:00 = no clock/CS; ff:ff:ff = MISO idle high. */
    if (w25q32_read_jedec_id(id) != ARM_DRIVER_OK) {
        return 0;
    }
    return (id[0] == W25_JEDEC_MANU) && (id[1] == W25_JEDEC_TYPE) &&
           (id[2] == W25_JEDEC_CAP);
}

static int32_t Flash_Initialize(ARM_Flash_SignalEvent_t cb_event)
{
    uint8_t id[3] = {0};
    uint32_t i;
    uint8_t last_id[3] = {0};

    ARG_UNUSED(cb_event);
    if (spi_inited != 0U) {
        return ARM_DRIVER_OK;
    }
    SPI_FLASH0_STATUS.error = 0;
    SPI_FLASH0_STATUS.busy = 0;

    for (i = 0U; i < W25_GPIO_PROFILE_COUNT; i++) {
        const w25_gpio_cfg_t *cfg = &w25_gpio_profiles[i];

        if (spi_hw_init_cfg(cfg) != 0) {
            w25_restore_cfg(cfg, NULL);
            continue;
        }

        w25_wakeup_reset();
        if (w25_jedec_match(id) != 0) {
            w25_restore_unused(cfg);
            SPI_FLASH_LOG_INF("W25 GPIO %s JEDEC %02x:%02x:%02x",
                              cfg->name, id[0], id[1], id[2]);
            spi_inited = 1U;
            SPI_FLASH_LOG_INF("SPI Flash Interface initialized");
            return ARM_DRIVER_OK;
        }

        last_id[0] = id[0];
        last_id[1] = id[1];
        last_id[2] = id[2];
        SPI_FLASH_LOG_ERR("W25 GPIO %s JEDEC %02x:%02x:%02x (expect ef:40:16)",
                          cfg->name, id[0], id[1], id[2]);
        w25_restore_cfg(cfg, NULL);
    }

    SPI_FLASH0_STATUS.error = 1;
    SPI_FLASH_LOG_ERR("W25Q32 JEDEC mismatch - SPI NOR not ready (%02x:%02x:%02x)",
                      last_id[0], last_id[1], last_id[2]);
    return ARM_DRIVER_ERROR;
}

static int32_t Flash_Uninitialize(void)
{
    spi_inited = 0U;
    return ARM_DRIVER_OK;
}

static int32_t Flash_PowerControl(ARM_POWER_STATE state)
{
    ARG_UNUSED(state);
    return ARM_DRIVER_OK;
}

static int32_t Flash_ReadData(uint32_t addr, void *data, uint32_t cnt)
{
    if ((data == NULL) || (cnt == 0U) || (spi_inited == 0U)) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
    if ((addr + cnt) > SPI_FLASH_TOTAL_SIZE) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }

    SPI_FLASH0_STATUS.busy = 1;
    if (w25_stream_read(addr, data, cnt) != 0) {
        SPI_FLASH0_STATUS.busy = 0;
        SPI_FLASH0_STATUS.error = 1;
        return ARM_DRIVER_ERROR;
    }
#if defined(TFM_SPI_FLASH_IN_BL2)
    overlay_pending_magic(addr, data, cnt);
#endif
    SPI_FLASH0_STATUS.busy = 0;
    return (int32_t)cnt;
}

#if defined(TFM_SPI_FLASH_IN_BL2)
static int32_t Flash_ProgramData(uint32_t addr, const void *data, uint32_t cnt)
{
    /* BL2 never programs W25Q32. Trailer MAGIC is overlaid on read. */
    ARG_UNUSED(addr);
    ARG_UNUSED(data);
    ARG_UNUSED(cnt);
    return ARM_DRIVER_OK;
}
#else
static int32_t Flash_ProgramData(uint32_t addr, const void *data, uint32_t cnt)
{
    const uint8_t *src = data;
    uint32_t remaining = cnt;
    uint32_t off = addr;
    uint32_t chunk;

    if ((data == NULL) || (cnt == 0U) || (spi_inited == 0U)) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
    if (!is_slot_range(addr, cnt)) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }

    SPI_FLASH0_STATUS.busy = 1;
    while (remaining != 0U) {
        chunk = SPI_FLASH_PAGE_SIZE - (off & (SPI_FLASH_PAGE_SIZE - 1U));
        if (chunk > remaining) {
            chunk = remaining;
        }
        if (w25_page_program(off, src, chunk) != 0) {
            SPI_FLASH0_STATUS.busy = 0;
            SPI_FLASH0_STATUS.error = 1;
            return ARM_DRIVER_ERROR;
        }
        off += chunk;
        src += chunk;
        remaining -= chunk;
    }
    SPI_FLASH0_STATUS.busy = 0;
    return (int32_t)cnt;
}
#endif /* TFM_SPI_FLASH_IN_BL2 */

static int32_t w25_erase_cmd(uint32_t addr, uint8_t cmd_id, uint32_t len)
{
    uint8_t cmd[4];

    if ((spi_inited == 0U) || ((addr % len) != 0U)) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
    if (!is_slot_range(addr, len)) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }

    SPI_FLASH0_STATUS.busy = 1;
    if (w25_write_enable() != 0) {
        SPI_FLASH0_STATUS.busy = 0;
        SPI_FLASH0_STATUS.error = 1;
        return ARM_DRIVER_ERROR;
    }

    cmd[0] = cmd_id;
    cmd[1] = (uint8_t)(addr >> 16);
    cmd[2] = (uint8_t)(addr >> 8);
    cmd[3] = (uint8_t)addr;
    if (w25_cmd(cmd, sizeof(cmd), NULL, 0U, NULL, 0U) != 0) {
        SPI_FLASH0_STATUS.busy = 0;
        SPI_FLASH0_STATUS.error = 1;
        return ARM_DRIVER_ERROR;
    }
    if (w25_wait_ready() != 0) {
        SPI_FLASH0_STATUS.busy = 0;
        SPI_FLASH0_STATUS.error = 1;
        return ARM_DRIVER_ERROR;
    }
    SPI_FLASH0_STATUS.busy = 0;
    return ARM_DRIVER_OK;
}

static int32_t Flash_EraseSector(uint32_t addr)
{
    return w25_erase_cmd(addr, W25_CMD_SECTOR_ERASE_4K, SPI_FLASH_SECTOR_SIZE);
}

static int32_t Flash_EraseChip(void)
{
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static ARM_FLASH_STATUS Flash_GetStatus(void)
{
    return SPI_FLASH0_STATUS;
}

static ARM_FLASH_INFO *Flash_GetInfo(void)
{
    return &SPI_FLASH0_DEV_DATA;
}

ARM_DRIVER_FLASH TFM_Driver_SPI_FLASH0 = {
    Flash_GetVersion,
    Flash_GetCapabilities,
    Flash_Initialize,
    Flash_Uninitialize,
    Flash_PowerControl,
    Flash_ReadData,
    Flash_ProgramData,
    Flash_EraseSector,
    Flash_EraseChip,
    Flash_GetStatus,
    Flash_GetInfo
};

int32_t w25q32_init(void)
{
    return Flash_Initialize(NULL);
}

int32_t w25q32_read(uint32_t addr, void *buf, uint32_t len)
{
    int32_t ret = Flash_ReadData(addr, buf, len);

    return (ret < 0) ? ret : ARM_DRIVER_OK;
}

int32_t w25q32_write(uint32_t addr, const void *buf, uint32_t len)
{
    int32_t ret = Flash_ProgramData(addr, buf, len);

    return (ret < 0) ? ret : ARM_DRIVER_OK;
}

int32_t w25q32_erase_4k(uint32_t addr)
{
    return Flash_EraseSector(addr);
}

int32_t w25q32_erase_range(uint32_t addr, uint32_t len)
{
    int32_t rc;

    if ((len == 0U) || ((addr % SPI_FLASH_SECTOR_SIZE) != 0U) ||
        ((len % SPI_FLASH_SECTOR_SIZE) != 0U)) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
    if (!is_slot_range(addr, len)) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }

    while (len != 0U) {
        if (((addr % W25_BLOCK_64K) == 0U) && (len >= W25_BLOCK_64K)) {
            rc = w25_erase_cmd(addr, W25_CMD_BLOCK_ERASE_64K, W25_BLOCK_64K);
            if (rc != ARM_DRIVER_OK) {
                return rc;
            }
            addr += W25_BLOCK_64K;
            len -= W25_BLOCK_64K;
        } else {
            rc = Flash_EraseSector(addr);
            if (rc != ARM_DRIVER_OK) {
                return rc;
            }
            addr += SPI_FLASH_SECTOR_SIZE;
            len -= SPI_FLASH_SECTOR_SIZE;
        }
    }

    return ARM_DRIVER_OK;
}

int32_t w25q32_read_jedec_id(uint8_t id[3])
{
    uint8_t cmd = W25_CMD_JEDEC_ID;

    if (id == NULL) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
    if (w25_cmd(&cmd, 1U, NULL, 0U, id, 3U) != 0) {
        return ARM_DRIVER_ERROR;
    }
    return ARM_DRIVER_OK;
}
