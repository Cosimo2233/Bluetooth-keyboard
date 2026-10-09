#include "CONFIG.h"
#include "hiddev.h"
#include "hidkbdservice.h"
#include "usb_keyboard.h"
#include "keyboard_transport.h"

#define REPORT_QUEUE_SIZE 32
static uint8_t reports[REPORT_QUEUE_SIZE][8];
static uint8_t head, count;
static uint8_t ble_ready;
static uint8_t active;
static uint8_t ble_release_pending;
static uint32_t usb_session;
static uint32_t ble_session, observed_ble_session;
static const uint8_t released[8] = {0};

void KeyboardTransport_SetBleReady(uint8_t ready)
{
    if(ready != ble_ready) ++ble_session;
    ble_ready = ready;
    if(!ready) ble_release_pending = 0;
}

uint8_t KeyboardTransport_Update(void)
{
    uint8_t next = UsbKeyboard_Connected() ? KEYBOARD_USB :
                   (ble_ready ? KEYBOARD_BLE : KEYBOARD_WAITING);
    uint32_t next_session = UsbKeyboard_Session();
    uint8_t changed = next != active || (next == KEYBOARD_USB && next_session != usb_session) ||
                      (next == KEYBOARD_BLE && ble_session != observed_ble_session);
    usb_session = next_session;
    observed_ble_session = ble_session;
    if(changed)
    {
        /* Release the old BLE host even while USB owns subsequent typing.
         * Retry if its notification buffers are temporarily full. */
        if(active == KEYBOARD_BLE && ble_ready) ble_release_pending = 1;
        active = next;
        head = count = 0;
        if(active != KEYBOARD_WAITING) KeyboardTransport_Report(released);
    }
    return changed;
}

uint8_t KeyboardTransport_Active(void) {return active;}
uint8_t KeyboardTransport_HasRoom(void) {return count <= REPORT_QUEUE_SIZE - 2;}

void KeyboardTransport_Report(const uint8_t report[8])
{
    if(active == KEYBOARD_WAITING) return;
    /* Caller reserves room for a press/release pair before consuming keys. */
    if(count >= REPORT_QUEUE_SIZE) return;
    memcpy(reports[(head + count) % REPORT_QUEUE_SIZE], report, 8);
    reports[(head + count) % REPORT_QUEUE_SIZE][1] = 0;
    ++count;
    /* All-up/idle reports must not wake a sleeping computer by themselves. */
    if(active == KEYBOARD_USB)
    {
        for(uint8_t i = 0; i < 8; ++i)
            if(i != 1 && report[i]) {UsbKeyboard_RequestWakeup(); break;}
    }
}

void KeyboardTransport_Service(void)
{
    uint8_t sent = 0;
    /* Also service wakeup when reports are queued awaiting host resume. */
    UsbKeyboard_Poll(TMOS_GetSystemClock(), count == 0);
    if(ble_release_pending && ble_ready)
    {
        if(HidDev_Report(HID_RPT_ID_KEY_IN, HID_REPORT_TYPE_INPUT, 8, (uint8_t *)released) == SUCCESS)
            ble_release_pending = 0;
        else if(active == KEYBOARD_BLE) return;
    }
    if(!count) return;
    /* Re-check readiness before sending: an interrupt may have reset USB. */
    if(active == KEYBOARD_USB && UsbKeyboard_Ready() && UsbKeyboard_Session() == usb_session)
        sent = UsbKeyboard_Send(reports[head]);
    else if(active == KEYBOARD_BLE && ble_ready && !UsbKeyboard_Connected())
        sent = HidDev_Report(HID_RPT_ID_KEY_IN, HID_REPORT_TYPE_INPUT, 8, reports[head]) == SUCCESS;
    if(sent) {head = (head + 1) % REPORT_QUEUE_SIZE; --count;}
}
