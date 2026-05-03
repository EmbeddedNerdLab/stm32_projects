#include "stm32f4xx_hal.h"
#include "aes128.h"
#include "boot_proto.h"
#include <string.h>
#include <stdbool.h>

/* ── Peripheral handles ──────────────────────────────────────────────────── */
static UART_HandleTypeDef huart2;
static RTC_HandleTypeDef  hrtc;

/* ── Clock: HSI 16 MHz, no PLL ───────────────────────────────────────────── */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
    osc.HSIState       = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.LSIState       = RCC_LSI_ON;
    osc.PLL.PLLState   = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&osc);

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0);
}

/* ── UART2 (PA2/PA3, same as app UART) ───────────────────────────────────── */
static void UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = PROTO_BAUD;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

/* ── RTC: needed to access backup registers ──────────────────────────────── */
static void RTC_Init(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    hrtc.Instance            = RTC;
    hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv   = 127;
    hrtc.Init.SynchPrediv    = 255;
    hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
    /* Don't re-init if already configured — preserves backup registers */
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0x32F2U)
        return;
    __HAL_RCC_RTC_ENABLE();
    HAL_RTC_Init(&hrtc);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0x32F2U);
}

/* ── LD2 LED (PA5) — fast blink during update mode ──────────────────────── */
static void LED_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = GPIO_PIN_5;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}

static void LED_Toggle(void) { HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5); }
static void LED_On(void)     { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); }
static void LED_Off(void)    { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); }

/* ── UART byte I/O with timeout ──────────────────────────────────────────── */
static int uart_recv_byte(uint8_t *b, uint32_t timeout_ms)
{
    return (HAL_UART_Receive(&huart2, b, 1, timeout_ms) == HAL_OK) ? 0 : -1;
}

static void uart_send(const uint8_t *buf, uint16_t len)
{
    HAL_UART_Transmit(&huart2, buf, len, 1000);
}

static void uart_send_byte(uint8_t b)
{
    uart_send(&b, 1);
}

/* ── Flash helpers ───────────────────────────────────────────────────────── */
/* STM32F446RE sector map for app region (sectors 4-7, 128 KB each) */
static const uint32_t k_app_sectors[] = {
    FLASH_SECTOR_4, FLASH_SECTOR_5, FLASH_SECTOR_6, FLASH_SECTOR_7
};

static HAL_StatusTypeDef flash_erase_app(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t error = 0;
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.NbSectors    = 4;
    erase.Sector       = FLASH_SECTOR_4;
    return HAL_FLASHEx_Erase(&erase, &error);
}

static HAL_StatusTypeDef flash_write_word(uint32_t addr, uint32_t word)
{
    return HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, word);
}

/* ── Jump to application ─────────────────────────────────────────────────── */
static void jump_to_app(void)
{
    typedef void (*AppEntry_t)(void);

    uint32_t sp  = *(volatile uint32_t *)APP_FLASH_BASE;
    uint32_t pc  = *(volatile uint32_t *)(APP_FLASH_BASE + 4);

    /* Validate: stack pointer must be in SRAM range, PC must be in app flash */
    if ((sp < 0x20000000U) || (sp > 0x20020000U)) return;
    if ((pc < APP_FLASH_BASE) || (pc >= APP_FLASH_END)) return;

    /* Disable all interrupts, reset peripherals */
    __disable_irq();
    HAL_DeInit();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* Relocate vector table */
    SCB->VTOR = APP_FLASH_BASE;
    __DSB();
    __ISB();

    /* Set stack pointer and jump */
    __set_MSP(sp);
    ((AppEntry_t)pc)();
}

/* ── Receive exact N bytes ────────────────────────────────────────────────── */
static int recv_buf(uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    return (HAL_UART_Receive(&huart2, buf, (uint16_t)len, timeout_ms) == HAL_OK) ? 0 : -1;
}

/* ── Update mode: receive, decrypt, flash ────────────────────────────────── */
static void run_update(void)
{
    uint8_t cmd;
    StartFrame_t sf;
    AES128_ctx   aes;

    LED_Init();

    /* ── Wait for START frame ──────────────────────────────────────────────── */
    uart_send_byte(CMD_ACK);  /* signal bootloader is ready */

    uint32_t deadline = HAL_GetTick() + PROTO_TIMEOUT_MS;
    for (;;) {
        if (HAL_GetTick() > deadline) { LED_Off(); uart_send_byte(CMD_ERR); return; }
        LED_Toggle();  /* fast blink: visible "waiting for host" indication */
        if (uart_recv_byte(&cmd, 100) == 0 && cmd == CMD_START) break;
    }
    LED_On();  /* solid ON: receiving + flashing in progress */

    if (recv_buf((uint8_t*)&sf, sizeof(sf), PROTO_TIMEOUT_MS) != 0) {
        LED_Off(); uart_send_byte(CMD_ERR); return;
    }

    /* Verify START frame CRC16 */
    uint16_t crc = crc16_update(0xFFFF, (uint8_t*)&sf, sizeof(sf) - 2);
    if (crc != sf.frame_crc16) { LED_Off(); uart_send_byte(CMD_NAK); return; }

    /* Reject absurd sizes */
    if (sf.fw_size == 0 || sf.fw_size > APP_FLASH_SIZE) {
        LED_Off(); uart_send_byte(CMD_ERR); return;
    }

    /* Init AES with embedded key, build full 16-byte counter from nonce */
    AES128_init(&aes, k_aes_key);
    uint8_t counter[16];
    memcpy(counter, sf.nonce, 12);
    counter[12] = 0; counter[13] = 0; counter[14] = 0; counter[15] = 0;

    uart_send_byte(CMD_ACK);

    /* ── Erase application sectors ─────────────────────────────────────────── */
    HAL_FLASH_Unlock();
    if (flash_erase_app() != HAL_OK) {
        HAL_FLASH_Lock();
        LED_Off(); uart_send_byte(CMD_ERR);
        return;
    }
    /* Second ACK: tells host that erase is done and we are ready for data.
     * Host must wait for this before sending any CMD_DATA frames because
     * incoming bytes are silently overrun while the flash erase blocks. */
    uart_send_byte(CMD_ACK);

    /* ── Receive DATA frames ───────────────────────────────────────────────── */
    uint32_t written   = 0;
    uint32_t crc32_acc = 0;
    uint8_t  enc_buf[PROTO_CHUNK];
    uint8_t  plain_buf[PROTO_CHUNK];

    while (written < sf.fw_size) {
        /* Expect DATA or END command byte */
        if (uart_recv_byte(&cmd, PROTO_TIMEOUT_MS) != 0) {
            HAL_FLASH_Lock(); uart_send_byte(CMD_ERR); return;
        }

        if (cmd == CMD_END) break;
        if (cmd != CMD_DATA) {
            HAL_FLASH_Lock(); LED_Off(); uart_send_byte(CMD_ERR); return;
        }

        /* How many payload bytes in this chunk? */
        uint32_t remaining = sf.fw_size - written;
        uint32_t chunk = (remaining < PROTO_CHUNK) ? remaining : PROTO_CHUNK;

        /* Receive encrypted chunk + CRC16 (full PROTO_CHUNK bytes always sent) */
        if (recv_buf(enc_buf, PROTO_CHUNK, PROTO_TIMEOUT_MS) != 0) {
            HAL_FLASH_Lock(); LED_Off(); uart_send_byte(CMD_ERR); return;
        }
        uint16_t rx_crc16;
        if (recv_buf((uint8_t*)&rx_crc16, 2, PROTO_TIMEOUT_MS) != 0) {
            HAL_FLASH_Lock(); LED_Off(); uart_send_byte(CMD_ERR); return;
        }

        /* Verify frame CRC16 over encrypted bytes */
        uint16_t calc_crc = crc16_update(0xFFFF, enc_buf, PROTO_CHUNK);
        if (calc_crc != rx_crc16) {
            HAL_FLASH_Lock(); LED_Off(); uart_send_byte(CMD_NAK); return;
        }
        LED_Toggle();  /* toggle each chunk so you see activity */

        /* Decrypt */
        AES128_ctr_xcrypt(&aes, counter, enc_buf, plain_buf, PROTO_CHUNK);

        /* Update CRC32 over only the valid plaintext bytes */
        crc32_acc = crc32_update(crc32_acc, plain_buf, chunk);

        /* Flash word-by-word (word = 4 bytes) */
        for (uint32_t i = 0; i < chunk; i += 4) {
            uint32_t word;
            uint32_t avail = chunk - i;
            if (avail >= 4) {
                memcpy(&word, plain_buf + i, 4);
            } else {
                word = 0xFFFFFFFFU;
                memcpy(&word, plain_buf + i, avail);
            }
            if (flash_write_word(APP_FLASH_BASE + written + i, word) != HAL_OK) {
                HAL_FLASH_Lock(); uart_send_byte(CMD_ERR); return;
            }
        }

        written += chunk;
        uart_send_byte(CMD_ACK);
    }

    HAL_FLASH_Lock();

    /* ── Verify CRC32 ──────────────────────────────────────────────────────── */
    if (written != sf.fw_size || crc32_acc != sf.fw_crc32) {
        LED_Off(); uart_send_byte(CMD_ERR);
        return;
    }

    /* 3 quick blinks = success */
    for (int i = 0; i < 6; i++) { LED_Toggle(); HAL_Delay(80); }
    LED_Off();

    uart_send_byte(CMD_ACK);

    /* ── Clear update flag and reboot into new app ─────────────────────────── */
    HAL_PWR_EnableBkUpAccess();
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_UPDATE_REG, 0x00000000U);
    HAL_Delay(100);
    NVIC_SystemReset();
}

/* ── DFU pin: PC13 (Nucleo USER button, active LOW) ─────────────────────── */
static bool dfu_pin_pressed(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin  = GPIO_PIN_13;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &g);
    HAL_Delay(5); /* debounce */
    return (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET);
}

/* ── Entry point ─────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    RTC_Init();

    /* Enter update mode if:
     *   (a) app requested it via BKP magic, OR
     *   (b) USER button (PC13) is held at boot  — hardware DFU override     */
    HAL_PWR_EnableBkUpAccess();
    uint32_t magic      = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_UPDATE_REG);
    bool     dfu_forced = dfu_pin_pressed();

    if (magic == UPDATE_MAGIC || dfu_forced) {
        /* Persist the flag so retries survive even if button is released */
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_UPDATE_REG, UPDATE_MAGIC);

        /* Loop until a successful update resets the MCU.
         * run_update() only returns on failure. */
        UART_Init();
        for (;;) {
            run_update();
            HAL_Delay(1000);
        }
    }

    /* Boot existing application */
    jump_to_app();

    /* Should never reach here — app flash is invalid, blink PA5 rapidly */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = GPIO_PIN_5;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    for (;;) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(100);
    }
}

/* ── Required HAL callbacks ──────────────────────────────────────────────── */
void SysTick_Handler(void) { HAL_IncTick(); }
