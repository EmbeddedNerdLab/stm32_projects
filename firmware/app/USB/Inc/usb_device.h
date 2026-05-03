#ifndef __USB_DEVICE_H
#define __USB_DEVICE_H

#include <stdint.h>

void USB_Device_Init(void);
int  usb_write(const uint8_t *data, uint16_t len);
int  usb_printf(const char *fmt, ...);

#endif /* __USB_DEVICE_H */
