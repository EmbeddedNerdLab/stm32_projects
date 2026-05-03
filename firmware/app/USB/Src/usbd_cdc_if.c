/*
 * usbd_cdc_if.c — CDC application interface
 *
 * TX flow: usb_write() → CDC_Transmit_FS() → USBD_CDC_TransmitPacket()
 *          → USB DMA → CDC_TransmitCplt_FS() → gives xTxDoneSem
 *
 * RX flow: host sends data → CDC_Receive_FS() → stored in UserRxBuffer
 *          → USBD_CDC_ReceivePacket() re-arms next RX
 *
 * Buffers are 4-byte aligned — required for OTG FS internal DMA.
 */

#include "usbd_cdc_if.h"
#include "usb_device.h"

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *Len);
static int8_t CDC_TransmitCplt_FS(uint8_t *pbuf, uint32_t *Len, uint8_t epnum);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
    CDC_TransmitCplt_FS
};

/* Buffers — must be 4-byte aligned for OTG DMA */
static uint8_t UserRxBufferFS[CDC_RX_BUF_SIZE] __attribute__((aligned(4)));

extern USBD_HandleTypeDef hUsbDeviceFS;

static int8_t CDC_Init_FS(void)
{
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    return USBD_OK;
}

static int8_t CDC_DeInit_FS(void)
{
    return USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    (void)length;
    (void)pbuf;
    (void)cmd;
    /* Accept all control requests silently (line coding, DTR/RTS, etc.) */
    return USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *Len)
{
    (void)pbuf;
    (void)Len;
    /* Re-arm RX for next packet */
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

static int8_t CDC_TransmitCplt_FS(uint8_t *pbuf, uint32_t *Len, uint8_t epnum)
{
    (void)pbuf;
    (void)Len;
    (void)epnum;
    CDC_TxCplt_Notify();
    return USBD_OK;
}

USBD_StatusTypeDef CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
    USBD_CDC_HandleTypeDef *hcdc =
        (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;

    if (hcdc == NULL)
        return USBD_FAIL;

    if (hcdc->TxState != 0U)
        return USBD_BUSY;

    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
    return (USBD_StatusTypeDef)USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}
