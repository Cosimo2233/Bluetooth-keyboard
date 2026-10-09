#ifndef KEYBOARD_TRANSPORT_H
#define KEYBOARD_TRANSPORT_H
#include <stdint.h>

enum {KEYBOARD_WAITING, KEYBOARD_BLE, KEYBOARD_USB};
/* Called from TMOS/main context, never from an ISR. */
void KeyboardTransport_SetBleReady(uint8_t ready);
uint8_t KeyboardTransport_Update(void);
uint8_t KeyboardTransport_Active(void);
uint8_t KeyboardTransport_HasRoom(void);
void KeyboardTransport_Report(const uint8_t report[8]);
void KeyboardTransport_Service(void);
#endif
