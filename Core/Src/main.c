/*
 * main.c — STM32F446RE Nucleo FreeRTOS LED blink demo
 *
 * Blinks LD2 (PA5) at 1 Hz using a FreeRTOS task.
 *
 * Clock configuration:
 *   HSE (8 MHz ST-LINK MCO, BYPASS mode)
 *   → PLL: M=4, N=180, P=2  → SYSCLK = 180 MHz
 *   AHB  (HCLK)  = 180 MHz  (÷1)
 *   APB1 (PCLK1) =  45 MHz  (÷4)  [max 45 MHz]
 *   APB2 (PCLK2) =  90 MHz  (÷2)  [max 90 MHz]
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/* ── Private function prototypes ─────────────────────────────────────────── */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void vLedBlinkTask(void *pvParameters);

/* ─────────────────────────────────────────────────────────────────────────── */
int main(void)
{
    /* Reset all peripherals, initialise Flash interface and SysTick.
     * SysTick is configured to 1 ms here; FreeRTOS overrides it when
     * vTaskStartScheduler() fires the first SVC.                          */
    HAL_Init();

    /* Configure system clock: HSE (bypass) → PLL → 180 MHz               */
    SystemClock_Config();

    /* Configure PA5 (LD2) as push-pull output                             */
    MX_GPIO_Init();

    /* Create the LED blink task                                            */
    BaseType_t xResult = xTaskCreate(
        vLedBlinkTask,           /* Task function                          */
        "LED_Blink",             /* Task name (for debugging)              */
        configNORMAL_STACK_SIZE, /* Stack depth in words (256 = 1024 B)   */
        NULL,                    /* No parameters                          */
        1,                       /* Priority (above idle)                  */
        NULL                     /* No task handle needed                  */
    );

    if (xResult != pdPASS)
    {
        Error_Handler();
    }

    /* Start the scheduler — does not return under normal operation.
     * vPortSVCHandler fires the first task via SVC.                       */
    vTaskStartScheduler();

    /* If we reach here, heap was too small to create the idle task.       */
    Error_Handler();

    for (;;) {}
}

/* ── LED blink task ──────────────────────────────────────────────────────── */
static void vLedBlinkTask(void *pvParameters)
{
    (void) pvParameters;

    for (;;)
    {
        HAL_GPIO_TogglePin(LD2_GPIO_PORT, LD2_PIN);
        /* pdMS_TO_TICKS converts ms to ticks using configTICK_RATE_HZ.
         * At 1000 Hz tick rate: 500 ms = 500 ticks.                       */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ── System clock configuration ─────────────────────────────────────────── */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Enable PWR clock; set voltage scale 1 (required for 180 MHz).
     * Scale 1 (VOS = 0b11) is mandatory when SYSCLK > 168 MHz.           */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* PLL configuration:
     *   VCO input  = HSE / PLLM = 8 / 4 = 2 MHz
     *   VCO output = 2 * PLLN   = 2 * 180 = 360 MHz  (336–433 MHz range ✓)
     *   SYSCLK     = VCO / PLLP = 360 / 2 = 180 MHz  ✓
     *   USB/SDIO   = VCO / PLLQ = 360 / 8 = 45 MHz   (≤ 48 MHz ✓)       */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_BYPASS; /* MCO from ST-LINK */
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 4;
    RCC_OscInitStruct.PLL.PLLN       = 180;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 8;
    RCC_OscInitStruct.PLL.PLLR       = 2;  /* STM32F446 has PLLR */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* Enable Over-Drive to reach 180 MHz — mandatory on STM32F446
     * when SYSCLK > 168 MHz.                                              */
    if (HAL_PWREx_EnableOverDrive() != HAL_OK)
    {
        Error_Handler();
    }

    /* Configure bus dividers.
     * Flash wait states: 5 cycles at 180 MHz, VDD = 3.3V
     * (reference manual Table 10).                                        */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  |
                                       RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1  |
                                       RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;  /* HCLK  = 180 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;    /* PCLK1 =  45 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;    /* PCLK2 =  90 MHz */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ── GPIO initialisation ─────────────────────────────────────────────────── */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    LD2_GPIO_CLK_ENABLE();

    /* Start with LED off */
    HAL_GPIO_WritePin(LD2_GPIO_PORT, LD2_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = LD2_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LD2_GPIO_PORT, &GPIO_InitStruct);
}

/* ── FreeRTOS application hooks ──────────────────────────────────────────── */

/* Called when a task stack overflow is detected (configCHECK_FOR_STACK_OVERFLOW = 2) */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void) xTask;
    (void) pcTaskName;
    Error_Handler();
}

/* Called when pvPortMalloc() fails (configUSE_MALLOC_FAILED_HOOK = 1) */
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
