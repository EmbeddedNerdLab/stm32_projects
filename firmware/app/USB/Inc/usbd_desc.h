#ifndef __USBD_DESC_H
#define __USBD_DESC_H

#include "usbd_def.h"

#define USBD_VID                  0x0483U
#define USBD_PID                  0x5740U
#define USBD_LANGID_STRING        0x0409U
#define USBD_MANUFACTURER_STRING  "STMicroelectronics"
#define USBD_PRODUCT_STRING       "STM32 CDC Console"
#define USBD_SERIALNUMBER_STRING  "001234567890"

extern USBD_DescriptorsTypeDef CDC_Desc;

#endif /* __USBD_DESC_H */
