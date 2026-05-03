#ifndef __USBD_CDC_IF_H
#define __USBD_CDC_IF_H

#include "usbd_cdc.h"

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

USBD_StatusTypeDef CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);
void               CDC_TxCplt_Notify(void);

#endif /* __USBD_CDC_IF_H */
