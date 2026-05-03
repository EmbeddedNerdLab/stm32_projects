#ifndef APP_EVENTS_H
#define APP_EVENTS_H

/* Event group bit definitions for xSysEventGroup.
 * Bits 24–31 are reserved by FreeRTOS internals; user bits use 0–23. */
#define EV_NEW_READING      ( 1U << 0 )  /* SensorRead_Task → any consumer       */
#define EV_LOG_DONE         ( 1U << 1 )  /* Logger_Task → diagnostic             */
#define EV_RTC_UPDATED      ( 1U << 2 )  /* RTCSync_Task → any consumer          */
#define EV_LIGHT_THRESH_HI  ( 1U << 3 )  /* SensorRead_Task → Buzzer_Task (ON)   */
#define EV_LIGHT_THRESH_LO  ( 1U << 4 )  /* SensorRead_Task → Buzzer_Task (OFF)  */
#define EV_EEPROM_FULL      ( 1U << 5 )  /* Logger_Task → diagnostic (sticky)    */

/* BH1750 raw count thresholds (lux = raw / 1.2, so raw = lux * 1.2) */
#define LIGHT_THRESH_HI_RAW  600U   /* ~500 lux: buzzer ON  */
#define LIGHT_THRESH_LO_RAW  480U   /* ~400 lux: buzzer OFF */

#endif /* APP_EVENTS_H */
