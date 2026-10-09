#ifndef USB_KEYBOARD_H
#define USB_KEYBOARD_H

#include <stdint.h>

/* USBHS controller, full-speed device on CH585 PB12 (D-) / PB13 (D+). */
void UsbKeyboard_Init(void);
uint8_t UsbKeyboard_Ready(void);
/* Includes an enumerated suspended host; Ready means transfers can run. */
uint8_t UsbKeyboard_Connected(void);
void UsbKeyboard_RequestWakeup(void);
uint32_t UsbKeyboard_Session(void);
uint8_t UsbKeyboard_Send(const uint8_t report[8]);
void UsbKeyboard_Poll(uint32_t now, uint8_t allow_idle_report);

#endif
