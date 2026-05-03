#ifndef APP_TASKS_H
#define APP_TASKS_H

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "sensor_record.h"

/* OS objects — defined in app_tasks.c, used across tasks */
extern QueueHandle_t      xLogQueue;
extern SemaphoreHandle_t  xI2CMutex;
extern SemaphoreHandle_t  xSPIMutex;
extern EventGroupHandle_t xSysEventGroup;

void App_Init(I2C_HandleTypeDef *hi2c, SPI_HandleTypeDef *hspi,
              TIM_HandleTypeDef *htim);

void vSensorReadTask(void *pvParameters);
void vLoggerTask(void *pvParameters);
void vBuzzerTask(void *pvParameters);
void vHeartbeatTask(void *pvParameters);
void vCommandTask(void *pvParameters);

#endif /* APP_TASKS_H */
