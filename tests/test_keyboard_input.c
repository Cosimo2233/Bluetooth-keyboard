#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "keyboard_transport.h"

#define CONST const
#define HID_KEYBOARD_IN_RPT_LEN 8
#define KEY_COUNT 48
#define START_TOUCH_KEY_PROCESS_EVT 1
#define START_SPECIAL_KEY_TIMEOUT_EVT 2
#define DEFAULT_SPECIAL_KEY_TIMEOUT 960
#define hidEmuTaskId 0
#define MS1_TO_SYSTEM_TIME(ms) ((ms) * 8 / 5)
#define PRINT(...) ((void)0)
#define tmos_memset memset
#define tmos_start_task(task, event, delay) ((void)(task), (void)(event), (void)(delay))
#define tmos_stop_task(task, event) ((void)(task), (void)(event))
#define tmos_clear_event(task, event) ((void)(task), (void)(event))

static uint8_t input_event, room = 1;
static uint8_t captured[16][8];
static unsigned captured_count, consumed;
uint8_t KeyboardTransport_Update(void) {return 0;}
uint8_t KeyboardTransport_Active(void) {return KEYBOARD_USB;}
uint8_t KeyboardTransport_HasRoom(void) {return room;}
void KeyboardTransport_Report(const uint8_t report[8])
{
    assert(captured_count < 16);
    memcpy(captured[captured_count++], report, 8);
}
static uint8_t touch_GetKey(void)
{
    uint8_t event = input_event;
    input_event = 0;
    ++consumed;
    return event;
}
static void touch_ClearKey(void) {input_event = 0;}

/* Generated from APP/hidkbd.c by run-tests.ps1; never a duplicate algorithm. */
#include "hidkbd_input.generated.tmp"

static void touch(uint8_t event)
{
    input_event = event;
    assert(process_input(START_TOUCH_KEY_PROCESS_EVT) == 0);
}

int main(void)
{
    /* ESC is key index zero, distinct from the no-event code zero. */
    touch(1);
    assert(captured_count == 1 && captured[0][2] == 0x29);
    touch(2);
    assert(captured_count == 2 && captured[1][2] == 0);
    touch(0);
    assert(captured_count == 2);

    /* Release ESC while another key remains held, then release that key. */
    touch(1); touch(4); touch(2);
    assert(captured_count == 5 && captured[4][2] == 0x14 && captured[4][3] == 0);
    touch(5);
    assert(captured[5][2] == 0);

    /* Preserve the original sequential modifier + ordinary key behavior. */
    touch(24 * 3 + 1); touch(24 * 3 + 2);
    assert(captured_count == 6);
    touch(1);
    assert(captured_count == 8 && captured[6][0] == 2 && captured[6][2] == 0x29);
    assert(captured[7][0] == 0 && captured[7][2] == 0);
    touch(2);

    /* Backpressure must leave the event in the touch FIFO for a later poll. */
    room = 0;
    unsigned previous = consumed;
    touch(4);
    assert(consumed == previous && input_event == 4);
    room = 1;
    assert(process_input(START_TOUCH_KEY_PROCESS_EVT) == 0);
    assert(captured[captured_count - 1][2] == 0x14);
    puts("PASS: real input branch ESC press/release, held keys, sequential modifier, queue backpressure");
    return 0;
}
