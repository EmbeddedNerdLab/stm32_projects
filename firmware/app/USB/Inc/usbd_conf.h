#ifndef __USBD_CONF_H
#define __USBD_CONF_H

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include <string.h>

#define USBD_MAX_NUM_INTERFACES        2U
#define USBD_MAX_NUM_CONFIGURATION     1U
#define USBD_MAX_STR_DESC_SIZ          512U
#define USBD_SELF_POWERED              1U
#define USBD_DEBUG_LEVEL               0U

#define USBD_malloc                    pvPortMalloc
#define USBD_free                      vPortFree
#define USBD_memset                    memset
#define USBD_memcpy                    memcpy
#define USBD_Delay                     HAL_Delay

#define DEVICE_FS                      0U

/* CDC application buffer sizes */
#define CDC_TX_BUF_SIZE                512U
#define CDC_RX_BUF_SIZE                512U

#endif /* __USBD_CONF_H */
