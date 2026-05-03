/*
 * usb_device.c — USB CDC thread-safe API
 *
 * usb_write / usb_printf are safe to call from any FreeRTOS task.
 * A mutex serialises concurrent callers; a binary semaphore blocks until
 * the USB DMA transfer completes (signalled from CDC_TransmitCplt_FS ISR).
 *
 * If the USB host is not connected (device not in CONFIGURED state) or the
 * transfer does not complete within 50 ms, the call returns -1 without
 * stalling the caller indefinitely.
 *
 * TX buffer is declared here with 4-byte alignment for OTG FS DMA.
 */

#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

USBD_HandleTypeDef hUsbDeviceFS;

static SemaphoreHandle_t xCDCTxMutex;
static SemaphoreHandle_t xCDCTxDone;

/* Aligned TX staging buffer — the DMA reads from this during transfer */
static uint8_t TxStageBuf[CDC_TX_BUF_SIZE] __attribute__((aligned(4)));

void USB_Device_Init(void)
{
    xCDCTxMutex = xSemaphoreCreateMutex();
    xCDCTxDone  = xSemaphoreCreateBinary();
    configASSERT(xCDCTxMutex != NULL);
    configASSERT(xCDCTxDone  != NULL);

    USBD_Init(&hUsbDeviceFS, &CDC_Desc, DEVICE_FS);
    USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC);
    USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS);
    USBD_Start(&hUsbDeviceFS);
}

/* Called from CDC_TransmitCplt_FS — USB ISR context */
void CDC_TxCplt_Notify(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xCDCTxDone, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

int usb_write(const uint8_t *data, uint16_t len)
{
    if (len == 0)
        return 0;

    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
        return -1;

    if (xSemaphoreTake(xCDCTxMutex, pdMS_TO_TICKS(50)) != pdTRUE)
        return -1;

    uint16_t chunk = (len > CDC_TX_BUF_SIZE) ? CDC_TX_BUF_SIZE : len;
    memcpy(TxStageBuf, data, chunk);

    /* Clear any stale semaphore token before starting the transfer */
    xSemaphoreTake(xCDCTxDone, 0);

    USBD_StatusTypeDef status = CDC_Transmit_FS(TxStageBuf, chunk);
    if (status != USBD_OK) {
        xSemaphoreGive(xCDCTxMutex);
        return -1;
    }

    /* Wait for DMA transfer to complete (TX complete ISR gives the semaphore) */
    if (xSemaphoreTake(xCDCTxDone, pdMS_TO_TICKS(100)) != pdTRUE) {
        /* Timeout: force-clear TxState so the next call isn't stuck */
        USBD_CDC_HandleTypeDef *hcdc =
            (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
        if (hcdc)
            hcdc->TxState = 0U;
        xSemaphoreGive(xCDCTxMutex);
        return -1;
    }

    xSemaphoreGive(xCDCTxMutex);
    return (int)chunk;
}

int usb_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0)
        return usb_write((uint8_t *)buf, (uint16_t)len);

    return len;
}
