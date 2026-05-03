/*
 * main.c — STM32F446RE multi-sensor FreeRTOS data logger
 *
 * Sensors on I2C1 (PB8/PB9 @ 400 kHz):
 *   MPU6050 @ 0x69  — accelerometer / gyroscope
 *   BME280  @ 0x76  — temperature / humidity / pressure
 *   BH1750  @ 0x23  — ambient light
 *   DS1307  @ 0x68  — real-time clock
 *
 * Storage on SPI2 (PB13/PB14/PB15, CS=PB12):
 *   25AA512 — 64 KB EEPROM circular log
 *
 * Actuator on TIM3 CH1 (PB4):
 *   Passive buzzer — PWM tone, activates when lux >= 500
 *
 * Clock: HSE 8 MHz bypass (ST-LINK MCO) → PLL → 180 MHz SYSCLK
 *        APB1 = 45 MHz, APB2 = 90 MHz
 */

#include "main.h"
#include "app_tasks.h"
#include "uart_console.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

/* ── Peripheral handles (declared extern in main.h) ─────────────────────── */
I2C_HandleTypeDef hi2c1;
SPI_HandleTypeDef hspi2;
TIM_HandleTypeDef htim3;

/* ── Private function prototypes ─────────────────────────────────────────── */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM3_Init(void);

/* ── Firmware update trigger ─────────────────────────────────────────────────
 * Write magic to RTC backup register and reset; bootloader sees it and enters
 * update mode waiting for the encrypted binary over UART.                    */
void App_RequestFirmwareUpdate(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    /* RTC backup register 1 — not touched by RTC time-keeping */
    RTC->BKP1R = 0xDEAD5AFEU;
    __DSB();
    NVIC_SystemReset();
}

/* ── HAL tick override ───────────────────────────────────────────────────────
 * SysTick_Handler is remapped to xPortSysTickHandler (FreeRTOS) which does not
 * call HAL_IncTick(), so uwTick stays at 0 and HAL_Delay hangs forever.
 * Returning xTaskGetTickCount() fixes all HAL timeout loops: xTickCount is
 * incremented by xPortSysTickHandler on every SysTick, even before the
 * scheduler starts, so this override works at all times. */
uint32_t HAL_GetTick(void)
{
    return (uint32_t)xTaskGetTickCount();
}

/* ─────────────────────────────────────────────────────────────────────────── */
int main(void)
{
    /* Relocate vector table — required when app is not at 0x08000000 */
    SCB->VTOR = 0x08010000U;
    __DSB();
    __ISB();

    /* Reset peripherals, init Flash interface, configure 1 ms SysTick.
     * Calls HAL_MspInit() which sets NVIC_PRIORITYGROUP_4.              */
    HAL_Init();

    /* HSE bypass → PLL → 180 MHz */
    SystemClock_Config();

    /* GPIO: PA5 (LD2) */
    MX_GPIO_Init();

    /* Peripherals */
    MX_I2C1_Init();
    MX_SPI2_Init();
    MX_TIM3_Init();

    /* UART2 debug console — PA2 TX → ST-LINK bridge → virtual COM port */
    UART_Console_Init();

    /* Create OS objects, init drivers, create FreeRTOS tasks */
    App_Init(&hi2c1, &hspi2, &htim3);

    /* Start scheduler — does not return */
    vTaskStartScheduler();

    Error_Handler();
    for (;;) {}
}

/* ── System clock ────────────────────────────────────────────────────────── */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSE bypass (ST-LINK MCO @ 8 MHz) + PLL → 180 MHz
     * VCO input = 8/4 = 2 MHz; VCO out = 2*180 = 360 MHz; SYSCLK = 360/2 = 180 MHz */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_BYPASS;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 4;
    RCC_OscInitStruct.PLL.PLLN       = 180;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 8;
    RCC_OscInitStruct.PLL.PLLR       = 2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    /* Over-Drive required above 168 MHz on STM32F446 */
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 180 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;     /* PCLK1 =  45 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;     /* PCLK2 =  90 MHz */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler();
}

/* ── GPIO ────────────────────────────────────────────────────────────────── */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    LD2_GPIO_CLK_ENABLE();
    HAL_GPIO_WritePin(LD2_GPIO_PORT, LD2_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = LD2_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LD2_GPIO_PORT, &GPIO_InitStruct);
}

/* ── I2C1 @ 400 kHz ──────────────────────────────────────────────────────── */
static void MX_I2C1_Init(void)
{
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 400000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

/* ── SPI2 @ ~5.6 MHz (APB1/8), Mode 0 ──────────────────────────────────── */
static void MX_SPI2_Init(void)
{
    hspi2.Instance               = SPI2;
    hspi2.Init.Mode              = SPI_MODE_MASTER;
    hspi2.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;   /* CPOL = 0 */
    hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;    /* CPHA = 0 */
    hspi2.Init.NSS               = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; /* 45 MHz / 8 = 5.625 MHz */
    hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial     = 10;
    if (HAL_SPI_Init(&hspi2) != HAL_OK) Error_Handler();
}

/* ── TIM3 CH1 PWM (buzzer default 440 Hz) ───────────────────────────────── */
static void MX_TIM3_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    /* TIM3 clock = APB1 × 2 = 90 MHz; PSC=89 → 1 MHz tick
     * Default ARR for 440 Hz: 1,000,000/440 - 1 = 2272               */
    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 89;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 2272;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) Error_Handler();

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 1136;  /* 50% duty */
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
        Error_Handler();
}

/* ── FreeRTOS application hooks ──────────────────────────────────────────── */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask; (void)pcTaskName;
    Error_Handler();
}

void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}

/* ── Error handler ───────────────────────────────────────────────────────── */
void Error_Handler(void)
{
    __disable_irq();
    for (;;) {}
}
