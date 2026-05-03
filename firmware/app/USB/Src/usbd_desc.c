#include "usbd_desc.h"
#include "usbd_conf.h"
#include "usbd_ctlreq.h"

/* USB device descriptor (18 bytes) */
static uint8_t USBD_DeviceDesc[USB_LEN_DEV_DESC] = {
    USB_LEN_DEV_DESC,          /* bLength */
    USB_DESC_TYPE_DEVICE,      /* bDescriptorType */
    0x00, 0x02,                /* bcdUSB = 2.00 */
    0x02,                      /* bDeviceClass = CDC */
    0x02,                      /* bDeviceSubClass */
    0x00,                      /* bDeviceProtocol */
    USB_MAX_EP0_SIZE,          /* bMaxPacketSize0 */
    LOBYTE(USBD_VID),
    HIBYTE(USBD_VID),          /* idVendor */
    LOBYTE(USBD_PID),
    HIBYTE(USBD_PID),          /* idProduct */
    0x00, 0x02,                /* bcdDevice = 2.00 */
    USBD_IDX_MFC_STR,          /* iManufacturer */
    USBD_IDX_PRODUCT_STR,      /* iProduct */
    USBD_IDX_SERIAL_STR,       /* iSerialNumber */
    USBD_MAX_NUM_CONFIGURATION /* bNumConfigurations */
};

static uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] = {
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING)
};

/* Working buffer for string conversion */
static uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ];

static uint8_t *USBD_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_DeviceDesc);
    return USBD_DeviceDesc;
}

static uint8_t *USBD_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_LangIDDesc);
    return USBD_LangIDDesc;
}

static uint8_t *USBD_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_PRODUCT_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_SERIALNUMBER_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)"CDC Config", USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)"CDC Interface", USBD_StrDesc, length);
    return USBD_StrDesc;
}

USBD_DescriptorsTypeDef CDC_Desc = {
    USBD_DeviceDescriptor,
    USBD_LangIDStrDescriptor,
    USBD_ManufacturerStrDescriptor,
    USBD_ProductStrDescriptor,
    USBD_SerialStrDescriptor,
    USBD_ConfigStrDescriptor,
    USBD_InterfaceStrDescriptor,
};
