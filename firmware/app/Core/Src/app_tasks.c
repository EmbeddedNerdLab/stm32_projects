#include "app_tasks.h"
#include "uart_console.h"
#include "app_events.h"
#include "drv_mpu6050.h"
#include "drv_bme280.h"
#include "drv_bh1750.h"
#include "drv_buzzer.h"
#include "drv_hcsr04.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "main.h"
#include <string.h>
#include <stdbool.h>

/* ── OS objects ──────────────────────────────────────────────────────────── */
QueueHandle_t      xLogQueue;
SemaphoreHandle_t  xI2CMutex;
SemaphoreHandle_t  xSPIMutex;
EventGroupHandle_t xSysEventGroup;

/* ── Peripheral / driver handles ─────────────────────────────────────────── */
static I2C_HandleTypeDef *hI2C;
static MPU6050_Handle_t   hMpu;
static BME280_Handle_t    hBme;
static BH1750_Handle_t    hBh;
static Buzzer_Handle_t    hBuz;
static HCSR04_Handle_t    hSonar;

/* ── App_Init ────────────────────────────────────────────────────────────── */
void App_Init(I2C_HandleTypeDef *hi2c, SPI_HandleTypeDef *hspi,
              TIM_HandleTypeDef *htim)
{
    hI2C = hi2c;
    (void)hspi;

    xI2CMutex      = xSemaphoreCreateMutex();
    xSysEventGroup = xEventGroupCreate();
    xLogQueue      = xQueueCreate(4, sizeof(SensorRecord_t));

    configASSERT(xI2CMutex      != NULL);
    configASSERT(xSysEventGroup != NULL);
    configASSERT(xLogQueue      != NULL);

    Buzzer_Init(&hBuz, htim, TIM_CHANNEL_1);
    HCSR04_Init(&hSonar, HCSR04_TRIG_PORT, HCSR04_TRIG_PIN,
                          HCSR04_ECHO_PORT, HCSR04_ECHO_PIN);

    BaseType_t r;
    r = xTaskCreate(vSensorReadTask, "SensorRead", 512, NULL, 1, NULL);
    configASSERT(r == pdPASS);
    r = xTaskCreate(vLoggerTask,     "Logger",     256, NULL, 1, NULL);
    configASSERT(r == pdPASS);
    r = xTaskCreate(vBuzzerTask,     "Buzzer",     128, NULL, 3, NULL);
    configASSERT(r == pdPASS);
    r = xTaskCreate(vHeartbeatTask,  "Heartbeat",  128, NULL, 1, NULL);
    configASSERT(r == pdPASS);
    r = xTaskCreate(vCommandTask,    "CmdRx",      128, NULL, 2, NULL);
    configASSERT(r == pdPASS);
}

/* ── vSensorReadTask ─────────────────────────────────────────────────────── */
void vSensorReadTask(void *pvParameters)
{
    (void)pvParameters;

    DRV_Status_t s;

    s = MPU6050_Init(&hMpu, hI2C, xI2CMutex);
    uart_printf("MPU6050 init: %s (addr=0x%02X)\r\n",
                s == DRV_OK ? "OK" : "FAIL", (unsigned)hMpu.i2c_addr);

    s = BME280_Init(&hBme, hI2C, xI2CMutex);
    uart_printf("BME280  init: %s\r\n", s == DRV_OK ? "OK" : "FAIL");

    s = BH1750_Init(&hBh, hI2C, xI2CMutex);
    uart_printf("BH1750  init: %s\r\n", s == DRV_OK ? "OK" : "FAIL");

    MPU6050_Data_t imu = {0};
    BME280_Data_t  env = {0};
    uint16_t       raw_lux = 0;
    uint16_t       dist_cm = 0;

    /* Hysteresis state: buzzer ON while lux < 10, OFF while lux >= 15 */
    bool buzzer_active = false;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint8_t    mpu_fail_count = 0;

    for (;;) {
        xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));

        if (MPU6050_ReadAll(&hMpu, &imu) != DRV_OK) {
            if (++mpu_fail_count >= 3) {
                /* I2C bus may be stuck — reset peripheral then reinit sensor */
                HAL_I2C_DeInit(hI2C);
                HAL_I2C_Init(hI2C);
                MPU6050_Init(&hMpu, hI2C, xI2CMutex);
                mpu_fail_count = 0;
                uart_printf("MPU6050: I2C recovery\r\n");
            }
        } else {
            mpu_fail_count = 0;
        }

        BME280_ReadAll(&hBme, &env);
        BH1750_ReadRaw(&hBh, &raw_lux);

        /* Temperature: cdeg → whole degrees and tenths */
        int32_t t_int  = env.temperature_cdeg / 100;
        int32_t t_frac = env.temperature_cdeg % 100;
        if (t_frac < 0) t_frac = -t_frac;

        /* Humidity: pct256 → %RH with one decimal */
        uint32_t hum_int  = env.humidity_pct256 / 256U;
        uint32_t hum_frac = ((uint32_t)env.humidity_pct256 % 256U) * 10U / 256U;

        /* Pressure: Pa → hPa (whole) */
        uint32_t hpa = env.pressure_pa / 100U;

        uart_printf("IMU  AX=%6d AY=%6d AZ=%6d  GX=%6d GY=%6d GZ=%6d\r\n",
                    imu.accel_x, imu.accel_y, imu.accel_z,
                    imu.gyro_x,  imu.gyro_y,  imu.gyro_z);
        /* lux = raw / 1.2 — integer approx: raw * 10 / 12 */
        uint32_t lux = (uint32_t)raw_lux * 10U / 12U;

        uart_printf("ENV  T=%ld.%02ld C  H=%lu.%lu%%  P=%lu hPa  LUX=%lu\r\n",
                    (long)t_int, (long)t_frac,
                    (unsigned long)hum_int, (unsigned long)hum_frac,
                    (unsigned long)hpa, (unsigned long)lux);

        HCSR04_Read(&hSonar, &dist_cm, env.temperature_cdeg);
        uart_printf("DIST CM=%u\r\n", (unsigned)dist_cm);

        /* Buzzer: ON when lux < 10, OFF when lux >= 15 (hysteresis) */
        if (!buzzer_active && lux < 10U) {
            buzzer_active = true;
            xEventGroupSetBits(xSysEventGroup, EV_LIGHT_THRESH_HI);
        } else if (buzzer_active && lux >= 15U) {
            buzzer_active = false;
            xEventGroupSetBits(xSysEventGroup, EV_LIGHT_THRESH_LO);
        }
    }
}

/* ── vLoggerTask ─────────────────────────────────────────────────────────── */
void vLoggerTask(void *pvParameters)
{
    (void)pvParameters;
    SensorRecord_t rec;
    for (;;) {
        if (xQueueReceive(xLogQueue, &rec, portMAX_DELAY) == pdTRUE)
            (void)rec;
    }
}

/* ── vCommandTask ────────────────────────────────────────────────────────── *
 * Listens for 2-byte host commands on UART RX.
 * Protocol: 0xAA <CMD>
 *   0xAA 0xB0 → reboot into bootloader (firmware update)
 */
/* Single-byte command — 0x05 (ENQ) never appears in printable sensor output */
#define UART_CMD_ENTER_BL  0x05U

void vCommandTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5));

        /* Drain all received bytes; STM32F4 UART has no FIFO so read quickly */
        while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
            uint8_t b = (uint8_t)(huart2.Instance->DR & 0xFFU);
            if (b == UART_CMD_ENTER_BL) {
                uart_printf("CMD: rebooting to bootloader\r\n");
                vTaskDelay(pdMS_TO_TICKS(50));
                App_RequestFirmwareUpdate();
            }
        }
        /* Clear overrun error flag if it was set while task was sleeping */
        __HAL_UART_CLEAR_OREFLAG(&huart2);
    }
}

/* ── vHeartbeatTask ──────────────────────────────────────────────────────── */
void vHeartbeatTask(void *pvParameters)
{
    (void)pvParameters;
    for (;;) {
        HAL_GPIO_TogglePin(LD2_GPIO_PORT, LD2_PIN);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ── vBuzzerTask ─────────────────────────────────────────────────────────── */
void vBuzzerTask(void *pvParameters)
{
    (void)pvParameters;

    Buzzer_On(&hBuz, 440);
    vTaskDelay(pdMS_TO_TICKS(200));
    Buzzer_Off(&hBuz);

    for (;;) {
        EventBits_t bits = xEventGroupWaitBits(
            xSysEventGroup,
            EV_LIGHT_THRESH_HI | EV_LIGHT_THRESH_LO,
            pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & EV_LIGHT_THRESH_HI)
            Buzzer_On(&hBuz, 440);
        else if (bits & EV_LIGHT_THRESH_LO)
            Buzzer_Off(&hBuz);
    }
}
