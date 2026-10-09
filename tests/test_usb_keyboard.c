#include <assert.h>
#include <stdio.h>
#include "CONFIG.h"
#include "../APP/usb_keyboard.c"
#include "../APP/keyboard_transport.c"

uint16_t mock_R16_CLK_SYS_CFG;
uint16_t mock_R16_PIN_CONFIG;
uint16_t mock_R16_U2EP_R_TOG_AUTO;
uint16_t mock_R16_U2EP_RX_EN;
uint16_t mock_R16_U2EP_T_TOG_AUTO;
uint16_t mock_R16_U2EP_TX_EN;
uint16_t mock_R16_U2EP0_RX_LEN;
uint16_t mock_R16_U2EP0_T_LEN;
uint16_t mock_R16_U2EP1_T_LEN;
uint32_t mock_R32_U2EP0_DMA;
uint32_t mock_R32_U2EP0_MAX_LEN;
uint32_t mock_R32_U2EP1_MAX_LEN;
uint32_t mock_R32_U2EP1_TX_DMA;
uint8_t mock_R8_U2EP0_RX_CTRL;
uint8_t mock_R8_U2EP0_TX_CTRL;
uint8_t mock_R8_U2EP1_TX_CTRL;
uint8_t mock_R8_USB2_BASE_MODE;
uint8_t mock_R8_USB2_CTRL;
uint8_t mock_R8_USB2_DEV_AD;
uint8_t mock_R8_USB2_INT_EN;
uint8_t mock_R8_USB2_INT_FG;
uint8_t mock_R8_USB2_INT_ST;
uint8_t mock_R8_USB2_MIS_ST;
uint8_t mock_R8_USBHS_PLL_CTRL;
uint8_t mock_R8_USB2_WAKE_CTRL;
static uint32_t clock_now;
static uint8_t ble_busy;
static uint8_t ble_reports[64][8];
static unsigned ble_count;

uint32_t TMOS_GetSystemClock(void) {return clock_now;}
uint8_t HidDev_Report(uint8_t id, uint8_t type, uint8_t len, uint8_t *data)
{
    assert(id == 0 && type == 1 && len == 8);
    if(ble_busy) return 1;
    assert(ble_count < 64);
    memcpy(ble_reports[ble_count++], data, 8);
    return SUCCESS;
}

static void token(uint8_t token_value, uint8_t length)
{
    R8_USB2_INT_FG = USBHS_UDIF_TRANSFER;
    if((token_value & MASK_UIS_TOKEN) == UIS_TOKEN_IN)
    {
        uint8_t endpoint = token_value & MASK_UIS_ENDP;
        R8_USB2_INT_ST = USBHS_UDIS_EP_DIR | endpoint;
        if(endpoint == 0) R8_U2EP0_TX_CTRL |= USBHS_UEP_T_DONE;
        else R8_U2EP1_TX_CTRL |= USBHS_UEP_T_DONE;
    }
    else
    {
        R8_USB2_INT_ST = 0;
        R16_U2EP0_RX_LEN = length;
        if((token_value & MASK_UIS_TOKEN) == UIS_TOKEN_SETUP)
            R8_U2EP0_RX_CTRL = USBHS_UEP_R_SETUP_IS | USBHS_UEP_R_DONE;
        else
            R8_U2EP0_RX_CTRL |= USBHS_UEP_R_TOG_MATCH | USBHS_UEP_R_DONE;
    }
    USB2_DEVICE_IRQHandler();
    if(R8_USB2_INT_ST & USBHS_UDIS_EP_DIR)
    {
        if((R8_USB2_INT_ST & USBHS_UDIS_EP_ID_MASK) == 0)
            assert(!(R8_U2EP0_TX_CTRL & USBHS_UEP_T_DONE));
        else assert(!(R8_U2EP1_TX_CTRL & USBHS_UEP_T_DONE));
    }
    else assert(!(R8_U2EP0_RX_CTRL & USBHS_UEP_R_DONE));
}

static void setup(uint8_t type, uint8_t req, uint16_t val, uint16_t index, uint16_t len)
{
    ep0_buffer[0] = type; ep0_buffer[1] = req;
    ep0_buffer[2] = val; ep0_buffer[3] = val >> 8;
    ep0_buffer[4] = index; ep0_buffer[5] = index >> 8;
    ep0_buffer[6] = len; ep0_buffer[7] = len >> 8;
    token(UIS_TOKEN_SETUP, 8);
}

static void configure(void)
{
    R8_USB2_MIS_ST = USBHS_UDMS_READY;
    setup(0, USB_SET_CONFIGURATION, 1, 0, 0);
    token(UIS_TOKEN_IN, 0);
    assert(UsbKeyboard_Ready());
}

static void test_enumeration(void)
{
    UsbKeyboard_Init();
    assert(!UsbKeyboard_Ready());
    assert(R16_U2EP_TX_EN == (RB_EP0_EN | RB_EP1_EN));
    assert(R16_U2EP_RX_EN == RB_EP0_EN);
    assert(R8_USB2_BASE_MODE == USBHS_UD_SPEED_FULL);
    assert((R16_PIN_CONFIG & RB_PIN_USB2_EN) != 0);
    assert((R16_PIN_CONFIG & RB_PIN_USB_EN) == 0);
    assert(R8_USBHS_PLL_CTRL == USBHS_PLL_EN);
    assert(R32_U2EP0_MAX_LEN == 64 && R32_U2EP1_MAX_LEN == 8);
    setup(0x80, USB_GET_DESCRIPTOR, 0x0100, 0, 8);
    assert(R16_U2EP0_T_LEN == 8 && ep0_buffer[0] == 18 && ep0_buffer[7] == 64);
    token(UIS_TOKEN_IN, 0); token(UIS_TOKEN_OUT, 0);
    setup(0, USB_SET_ADDRESS, 7, 0, 0);
    assert(R8_USB2_DEV_AD == 0); /* Address is applied after the status ACK. */
    token(UIS_TOKEN_IN, 0);
    assert(R8_USB2_DEV_AD == 7);
    setup(0x80, USB_GET_DESCRIPTOR, 0x0200, 0, 255);
    assert(R16_U2EP0_T_LEN == 34 && ep0_buffer[2] == 34 && ep0_buffer[4] == 1);
    assert(ep0_buffer[29] == 0x81 && ep0_buffer[31] == 8);
    assert(ep0_buffer[7] & 0x20); /* Remote wakeup capability. */
    token(UIS_TOKEN_IN, 0); token(UIS_TOKEN_OUT, 0);
    setup(0x81, USB_GET_DESCRIPTOR, 0x2200, 0, 255);
    assert(R16_U2EP0_T_LEN == sizeof(report_descriptor));
    assert(ep0_buffer[R16_U2EP0_T_LEN - 1] == 0xc0);
    token(UIS_TOKEN_IN, 0); token(UIS_TOKEN_OUT, 0);
    setup(0x80, USB_GET_DESCRIPTOR, 0x0302, 0x0409, 255);
    assert(R16_U2EP0_T_LEN == sizeof(product_descriptor));
    assert(ep0_buffer[0] == sizeof(product_descriptor));
    token(UIS_TOKEN_IN, 0); token(UIS_TOKEN_OUT, 0);
    configure();
    setup(0x21, HID_SET_PROTOCOL, 0, 0, 0); token(UIS_TOKEN_IN, 0);
    assert(configuration == 1 && protocol == 0);
    setup(0xa1, HID_GET_PROTOCOL, 0, 0, 1);
    assert(ep0_buffer[0] == 0 && R16_U2EP0_T_LEN == 1);
    token(UIS_TOKEN_IN, 0); token(UIS_TOKEN_OUT, 0);
    setup(0x21, HID_SET_REPORT, 0x0200, 0, 1);
    ep0_buffer[0] = 2; token(UIS_TOKEN_OUT, 1); token(UIS_TOKEN_IN, 0);
    assert(configuration == 1 && leds == 2); /* 0x09 must not be confused with SET_CONFIGURATION. */
    setup(0x21, HID_SET_IDLE, 5 << 8, 0, 0); token(UIS_TOKEN_IN, 0);
    assert(idle_rate == 5);
    setup(0x81, USB_GET_DESCRIPTOR, 0x2200, 1, 255);
    assert((R8_U2EP0_TX_CTRL & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_STALL);
    setup(0x80, USB_GET_DESCRIPTOR, 0x0600, 0, 10); /* Full speed only. */
    assert((R8_U2EP0_TX_CTRL & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_STALL);
    setup(0x02, USB_SET_FEATURE, 0, 0x81, 0); token(UIS_TOKEN_IN, 0);
    assert((R8_U2EP1_TX_CTRL & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_STALL);
    setup(0x02, USB_CLEAR_FEATURE, 0, 0x81, 0); token(UIS_TOKEN_IN, 0);
    assert((R8_U2EP1_TX_CTRL & (USBHS_UEP_T_RES_MASK | USBHS_UEP_T_TOG_DATA1)) == USBHS_UEP_T_RES_NAK);
    setup(0, USB_SET_CONFIGURATION, 0, 0, 0); token(UIS_TOKEN_IN, 0);
    assert(!UsbKeyboard_Ready());
}

static void test_routing(void)
{
    uint8_t press[8] = {2, 0, 4};
    unsigned saved;
    uint32_t old_session;
    assert(!KeyboardTransport_Update() && KeyboardTransport_Active() == KEYBOARD_WAITING);
    KeyboardTransport_Report(press);
    assert(count == 0); /* No historical keystrokes when nothing is connected. */
    KeyboardTransport_SetBleReady(1);
    assert(KeyboardTransport_Update() && KeyboardTransport_Active() == KEYBOARD_BLE);
    KeyboardTransport_Service();
    KeyboardTransport_Report(press);
    KeyboardTransport_Service();
    assert(ble_count == 2 && ble_reports[1][0] == 2 && ble_reports[1][2] == 4);

    /* A disconnect/reconnect between polls must not keep the old key state. */
    KeyboardTransport_Report(press);
    KeyboardTransport_SetBleReady(0);
    KeyboardTransport_SetBleReady(1);
    assert(KeyboardTransport_Update() && count == 1 && reports[head][2] == 0);
    KeyboardTransport_Service();
    ble_count = 2;

    ble_busy = 1;
    configure();
    assert(KeyboardTransport_Update() && KeyboardTransport_Active() == KEYBOARD_USB);
    KeyboardTransport_Service(); /* BLE release is busy, USB must still work. */
    assert(ble_release_pending && count == 0);
    token(UIS_TOKEN_IN | 1, 0);
    KeyboardTransport_Report(press);
    KeyboardTransport_Report(released);
    KeyboardTransport_Service();
    assert(ep1_buffer[0] == 2 && ep1_buffer[2] == 4 && count == 1);
    KeyboardTransport_Service(); /* Endpoint still busy: release stays queued. */
    assert(ep1_buffer[2] == 4 && count == 1);
    token(UIS_TOKEN_IN | 1, 0);
    KeyboardTransport_Service();
    assert(ep1_buffer[2] == 0 && count == 0);
    token(UIS_TOKEN_IN | 1, 0);
    ble_busy = 0;
    KeyboardTransport_Service();
    assert(!ble_release_pending && ble_count == 3 && ble_reports[2][2] == 0);
    saved = ble_count;
    KeyboardTransport_Report(press); KeyboardTransport_Service();
    assert(ble_count == saved); /* Typing has only one destination. */
    token(UIS_TOKEN_IN | 1, 0);

    KeyboardTransport_Report(press); /* Must be discarded on USB reset. */
    old_session = UsbKeyboard_Session();
    R8_USB2_INT_FG = USBHS_UDIF_BUS_RST; USB2_DEVICE_IRQHandler();
    assert(!UsbKeyboard_Ready() && UsbKeyboard_Session() != old_session);
    configure(); /* Reset and enumerate between two TMOS polls. */
    assert(KeyboardTransport_Update() && count == 1 && reports[head][2] == 0);
    KeyboardTransport_Service(); token(UIS_TOKEN_IN | 1, 0);

    R8_USB2_MIS_ST = USBHS_UDMS_READY | USBHS_UDMS_SUSPEND;
    R8_USB2_INT_FG = USBHS_UDIF_SUSPEND; USB2_DEVICE_IRQHandler();
    assert(!UsbKeyboard_Ready() && UsbKeyboard_Connected());
    assert(!KeyboardTransport_Update() && KeyboardTransport_Active() == KEYBOARD_USB);
    KeyboardTransport_Service();
    saved = ble_count;
    KeyboardTransport_Report(press); KeyboardTransport_Service();
    assert(ble_count == saved && count == 1); /* Keep the tap for the USB host. */
    R8_USB2_MIS_ST = USBHS_UDMS_READY;
    /* Resume must work even without a second suspend interrupt. */
    assert(UsbKeyboard_Ready());
    assert(!KeyboardTransport_Update() && KeyboardTransport_Active() == KEYBOARD_USB);
    KeyboardTransport_Service(); token(UIS_TOKEN_IN | 1, 0);
    assert(ep1_buffer[2] == 4 && count == 0);

    for(unsigned i = 0; i < REPORT_QUEUE_SIZE - 1; ++i) KeyboardTransport_Report(press);
    assert(!KeyboardTransport_HasRoom());
    for(unsigned i = 0; i < REPORT_QUEUE_SIZE - 1; ++i)
    {
        KeyboardTransport_Service(); token(UIS_TOKEN_IN | 1, 0);
    }
    assert(count == 0 && KeyboardTransport_HasRoom());
    KeyboardTransport_SetBleReady(0);
    R8_USB2_INT_FG = USBHS_UDIF_BUS_RST; USB2_DEVICE_IRQHandler();
    assert(KeyboardTransport_Update() && KeyboardTransport_Active() == KEYBOARD_WAITING);
    assert(count == 0);
}

static void test_usbhs_link(void)
{
    /* Battery powered MCU: loss of USBHS READY must release USB ownership
     * even before a bus-reset interrupt is delivered. */
    configure();
    assert(KeyboardTransport_Update() && KeyboardTransport_Active() == KEYBOARD_USB);
    R8_USB2_MIS_ST = 0;
    assert(!UsbKeyboard_Ready());
    assert(KeyboardTransport_Update() && KeyboardTransport_Active() == KEYBOARD_WAITING);
    R8_USB2_INT_FG = USBHS_UDIF_LINK_RDY;
    USB2_DEVICE_IRQHandler();
    assert(configuration == 0);
    R8_USB2_MIS_ST = USBHS_UDMS_READY;
    R8_USB2_INT_FG = USBHS_UDIF_LINK_RDY;
    USB2_DEVICE_IRQHandler();
    assert(!UsbKeyboard_Ready()); /* Reattachment needs enumeration again. */
    configure();
    R8_USB2_MIS_ST = USBHS_UDMS_READY | USBHS_UDMS_SLEEP;
    R8_USB2_INT_FG = USBHS_UDIF_BUS_SLEEP;
    USB2_DEVICE_IRQHandler();
    assert(!UsbKeyboard_Ready());
    R8_USB2_MIS_ST = USBHS_UDMS_READY;
    R8_USB2_INT_FG = USBHS_UDIF_BUS_SLEEP;
    USB2_DEVICE_IRQHandler();
    assert(UsbKeyboard_Ready());
}

static void set_remote_wakeup(uint8_t enabled)
{
    setup(0, enabled ? USB_SET_FEATURE : USB_CLEAR_FEATURE,
          USB_REQ_FEAT_REMOTE_WAKEUP, 0, 0);
    token(UIS_TOKEN_IN, 0);
    setup(0x80, USB_GET_STATUS, 0, 0, 2);
    assert(ep0_buffer[0] == (enabled ? 2 : 0) && ep0_buffer[1] == 0);
    token(UIS_TOKEN_IN, 0); token(UIS_TOKEN_OUT, 0);
}

static void test_idle_wakeup(void)
{
    const uint8_t press[8] = {0, 0, 4};
    uint32_t old_session;
    configure();
    set_remote_wakeup(1);
    KeyboardTransport_Update();
    KeyboardTransport_Service(); token(UIS_TOKEN_IN | 1, 0);
    old_session = UsbKeyboard_Session();
    R8_USB2_WAKE_CTRL = 0;
    clock_now = MS1_TO_SYSTEM_TIME(60000);
    /* READY temporarily low during sleep + LINK_RDY used to erase config. */
    R8_USB2_MIS_ST = USBHS_UDMS_SUSPEND;
    R8_USB2_INT_FG = USBHS_UDIF_SUSPEND | USBHS_UDIF_LINK_RDY;
    USB2_DEVICE_IRQHandler();
    assert(configuration == 1 && UsbKeyboard_Session() == old_session);
    assert(!KeyboardTransport_Update() && KeyboardTransport_Active() == KEYBOARD_USB);
    KeyboardTransport_Service();
    assert(!R8_USB2_WAKE_CTRL); /* Idle alone does not wake. */
    KeyboardTransport_Report(press);
    KeyboardTransport_Report(released); /* Complete quick tap while suspended. */
    clock_now += MS1_TO_SYSTEM_TIME(4);
    KeyboardTransport_Service();
    assert(!R8_USB2_WAKE_CTRL && count == 2);
    ++clock_now;
    ++clock_now;
    KeyboardTransport_Service();
    assert(R8_USB2_WAKE_CTRL & USBHS_UD_UD_REMOTE_WKUP);
    R8_USB2_WAKE_CTRL = 0; /* Hardware clears the pulse. */
    clock_now += MS1_TO_SYSTEM_TIME(100);
    KeyboardTransport_Service();
    assert(!R8_USB2_WAKE_CTRL && count == 2); /* One pulse per suspend. */
    R8_USB2_MIS_ST = USBHS_UDMS_READY; /* No resume IRQ. */
    assert(!KeyboardTransport_Update());
    KeyboardTransport_Service();
    assert(ep1_buffer[2] == 4 && count == 1);
    token(UIS_TOKEN_IN | 1, 0);
    KeyboardTransport_Service();
    assert(ep1_buffer[2] == 0 && count == 0);
    token(UIS_TOKEN_IN | 1, 0);

    /* Disabled wakeup: retain reports but never signal the host. */
    set_remote_wakeup(0);
    R8_USB2_MIS_ST = USBHS_UDMS_READY | USBHS_UDMS_SLEEP;
    KeyboardTransport_Service();
    KeyboardTransport_Report(press);
    clock_now += MS1_TO_SYSTEM_TIME(100);
    KeyboardTransport_Service();
    assert(!R8_USB2_WAKE_CTRL && count == 1);
    R8_USB2_INT_FG = USBHS_UDIF_BUS_RST; USB2_DEVICE_IRQHandler();
    assert(!remote_wakeup && !wake_requested && !UsbKeyboard_Connected());
    KeyboardTransport_Update();
}

static void test_charger(void)
{
    /* Supply/PHY status without SET_CONFIGURATION is never a USB keyboard. */
    UsbKeyboard_Init();
    R8_USB2_MIS_ST = USBHS_UDMS_READY;
    assert(!UsbKeyboard_Connected());
    KeyboardTransport_SetBleReady(1);
    KeyboardTransport_Update();
    assert(KeyboardTransport_Active() == KEYBOARD_BLE);
    configure();
    KeyboardTransport_Update();
    assert(KeyboardTransport_Active() == KEYBOARD_USB);
    R8_USB2_MIS_ST = 0; /* Actual disconnect with battery power retained. */
    R8_USB2_INT_FG = USBHS_UDIF_LINK_RDY; USB2_DEVICE_IRQHandler();
    assert(configuration == 0);
    R8_USB2_MIS_ST = USBHS_UDMS_READY;
    KeyboardTransport_Update();
    assert(KeyboardTransport_Active() == KEYBOARD_BLE); /* Charger, no enum. */
    configure();
    R8_USB2_MIS_ST = 0; /* Missing disconnect IRQ is caught by main polling. */
    UsbKeyboard_Poll(clock_now, 0);
    assert(configuration == 0);
    KeyboardTransport_SetBleReady(0);
    KeyboardTransport_Update();
    assert(KeyboardTransport_Active() == KEYBOARD_WAITING);
}

int main(void)
{
    test_enumeration();
    test_routing();
    test_usbhs_link();
    test_idle_wakeup();
    test_charger();
    puts("PASS: USBHS init, enumeration, HID controls, ordered reports, USB priority, reset, queue pressure, idle/resume, remote wakeup, charger");
    return 0;
}
