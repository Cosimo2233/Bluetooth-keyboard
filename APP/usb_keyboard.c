#include "CONFIG.h"
#include "usb_keyboard.h"

/* Boot keyboard, no report IDs; the same eight bytes are used by BLE. */
static const uint8_t report_descriptor[] = {
    0x05, 0x01, 0x09, 0x06, 0xa1, 0x01,
    0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
    0x91, 0x02, 0x95, 0x03, 0x75, 0x01, 0x91, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xc0
};
/* Development VID/PID from WCH's CompoundDev example. */
static const uint8_t device_descriptor[] = {
    18, 1, 0x10, 0x01, 0, 0, 0, 64,
    0x3d, 0x41, 0x07, 0x21, 0x02, 0x01, 1, 2, 0, 1
};
static const uint8_t config_descriptor[] = {
    9, 2, 34, 0, 1, 1, 0, 0xa0, 50,
    9, 4, 0, 0, 1, 3, 1, 1, 0,
    9, 0x21, 0x11, 0x01, 0, 1, 0x22, sizeof(report_descriptor), 0,
    7, 5, 0x81, 3, 8, 0, 10
};
static const uint8_t lang_descriptor[] = {4, 3, 0x09, 0x04};
static const uint8_t manufacturer_descriptor[] = {
    8, 3, 'W', 0, 'C', 0, 'H', 0
};
static const uint8_t product_descriptor[] = {
    26, 3, 'H', 0, 'I', 0, 'D', 0, ' ', 0,
    'K', 0, 'e', 0, 'y', 0, 'b', 0, 'o', 0, 'a', 0, 'r', 0, 'd', 0
};

static __attribute__((aligned(4))) uint8_t ep0_buffer[64];
/* USBHS has separate RX/TX DMA addresses. EP1 is IN only. */
static __attribute__((aligned(4))) uint8_t ep1_buffer[64];
static volatile uint8_t configuration;
static volatile uint8_t remote_wakeup;
static volatile uint8_t wake_requested;
static volatile uint8_t wake_sent;
static volatile uint8_t suspend_observed;
static uint32_t suspend_time;
static volatile uint32_t session;
static volatile uint8_t idle_rate;
static uint8_t protocol = 1;
static uint8_t leds;
static uint8_t current_report[8];
static uint32_t last_report_time;

enum {EP0_IDLE, EP0_DATA_IN, EP0_DATA_OUT, EP0_STATUS_IN, EP0_STATUS_OUT};
static uint8_t ep0_phase;
static uint8_t request;
static uint8_t request_type;
static uint16_t value;
static const uint8_t *ep0_source;
static uint16_t ep0_remaining;
static uint8_t ep0_zlp;
static uint8_t reply[8];

static void reset_endpoint(void)
{
    R16_U2EP1_T_LEN = 0;
    R8_U2EP1_TX_CTRL = USBHS_UEP_T_RES_NAK;
    memset(current_report, 0, sizeof(current_report));
}

static void stall(void)
{
    ep0_phase = EP0_IDLE;
    R16_U2EP0_T_LEN = 0;
    R8_U2EP0_RX_CTRL = USBHS_UEP_R_RES_STALL;
    R8_U2EP0_TX_CTRL = USBHS_UEP_T_RES_STALL;
}

static void load_control_packet(void)
{
    uint8_t length = ep0_remaining > 64 ? 64 : ep0_remaining;
    if(length)
    {
        memcpy(ep0_buffer, ep0_source, length);
        ep0_source += length;
        ep0_remaining -= length;
    }
    else
    {
        ep0_zlp = 0;
    }
    R16_U2EP0_T_LEN = length;
}

static void control_read(const uint8_t *data, uint16_t length, uint16_t requested)
{
    ep0_source = data;
    ep0_remaining = length < requested ? length : requested;
    ep0_zlp = (length < requested && (length % 64) == 0);
    ep0_phase = requested ? EP0_DATA_IN : EP0_STATUS_OUT;
    load_control_packet();
    R8_U2EP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_ACK;
    R8_U2EP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 |
                        (requested ? USBHS_UEP_T_RES_ACK : USBHS_UEP_T_RES_NAK);
}

static void control_status(void)
{
    ep0_phase = EP0_STATUS_IN;
    R16_U2EP0_T_LEN = 0;
    R8_U2EP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_NAK;
    R8_U2EP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_ACK;
}

static void setup_request(void)
{
    /* Capture all fields before the DMA buffer is reused for the reply. */
    uint8_t type = ep0_buffer[0];
    request_type = type;
    uint16_t index = ep0_buffer[4] | ((uint16_t)ep0_buffer[5] << 8);
    uint16_t length = ep0_buffer[6] | ((uint16_t)ep0_buffer[7] << 8);
    request = ep0_buffer[1];
    value = ep0_buffer[2] | ((uint16_t)ep0_buffer[3] << 8);
    ep0_phase = EP0_IDLE;
    ep0_zlp = 0;

    if(request == USB_GET_DESCRIPTOR)
    {
        const uint8_t *descriptor = NULL;
        uint16_t size = 0;
        uint8_t descriptor_type = value >> 8;
        uint8_t number = value & 0xff;
        if(type == 0x80 && number == 0)
        {
            if(descriptor_type == USB_DESCR_TYP_DEVICE)
                {descriptor = device_descriptor; size = sizeof(device_descriptor);}
            else if(descriptor_type == USB_DESCR_TYP_CONFIG)
                {descriptor = config_descriptor; size = sizeof(config_descriptor);}
        }
        if(type == 0x80 && descriptor_type == USB_DESCR_TYP_STRING)
        {
            if(number == 0) {descriptor = lang_descriptor; size = sizeof(lang_descriptor);}
            else if(index == 0x0409 && number == 1)
                {descriptor = manufacturer_descriptor; size = sizeof(manufacturer_descriptor);}
            else if(index == 0x0409 && number == 2)
                {descriptor = product_descriptor; size = sizeof(product_descriptor);}
        }
        if(type == 0x81 && index == 0 && number == 0)
        {
            if(descriptor_type == USB_DESCR_TYP_HID)
                {descriptor = config_descriptor + 18; size = 9;}
            else if(descriptor_type == USB_DESCR_TYP_REPORT)
                {descriptor = report_descriptor; size = sizeof(report_descriptor);}
        }
        if(descriptor) control_read(descriptor, size, length);
        else stall();
        return;
    }

    if(type == 0x00 && index == 0 && length == 0 &&
       ((request == USB_SET_ADDRESS && value <= 127 && configuration == 0) ||
        (request == USB_SET_CONFIGURATION && value <= 1)))
    {
        control_status();
        return;
    }
    if(type == 0x80 && request == USB_GET_CONFIGURATION && value == 0 && index == 0 && length == 1)
    {
        reply[0] = configuration;
        control_read(reply, 1, length);
        return;
    }
    if(request == USB_GET_STATUS && value == 0 && length == 2 &&
       ((type == 0x80 && index == 0) || (configuration && type == 0x81 && index == 0) ||
        (type == 0x82 && (index == 0 || index == 0x80 || (configuration && index == 0x81)))))
    {
        reply[0] = type == 0x80 ? (remote_wakeup ? 2 : 0) :
                   (type == 0x82 && index == 0x81 &&
                    (R8_U2EP1_TX_CTRL & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_STALL);
        reply[1] = 0;
        control_read(reply, 2, length);
        return;
    }
    if(configuration && type == 0x00 && index == 0 &&
       value == USB_REQ_FEAT_REMOTE_WAKEUP && length == 0 &&
       (request == USB_CLEAR_FEATURE || request == USB_SET_FEATURE))
    {
        control_status();
        return;
    }
    if(configuration && type == 0x02 && index == 0x81 && value == 0 && length == 0 &&
       (request == USB_CLEAR_FEATURE || request == USB_SET_FEATURE))
    {
        control_status();
        return;
    }
    if(configuration && index == 0 && value == 0)
    {
        if(type == 0x81 && request == USB_GET_INTERFACE && length == 1)
            {reply[0] = 0; control_read(reply, 1, length); return;}
        if(type == 0x01 && request == USB_SET_INTERFACE && length == 0)
            {control_status(); return;}
    }
    if(configuration && index == 0)
    {
        if(type == 0xa1 && request == HID_GET_REPORT && (value & 0xff) == 0)
        {
            if((value >> 8) == 1)
                {memcpy(reply, current_report, 8); control_read(reply, 8, length); return;}
            if((value >> 8) == 2)
                {reply[0] = leds; control_read(reply, 1, length); return;}
        }
        if(type == 0xa1 && length == 1 && value == 0 &&
           (request == HID_GET_IDLE || request == HID_GET_PROTOCOL))
        {
            reply[0] = request == HID_GET_IDLE ? idle_rate : protocol;
            control_read(reply, 1, length);
            return;
        }
        if(type == 0x21 && length == 0 &&
           ((request == HID_SET_IDLE && (value & 0xff) == 0) ||
            (request == HID_SET_PROTOCOL && value <= 1)))
        {
            control_status();
            return;
        }
        if(type == 0x21 && request == HID_SET_REPORT && value == 0x0200 && length == 1)
        {
            ep0_phase = EP0_DATA_OUT;
            R8_U2EP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_ACK;
            R8_U2EP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
            return;
        }
    }
    stall();
}

static void control_in_complete(void)
{
    if(ep0_phase == EP0_DATA_IN)
    {
        if(ep0_remaining || ep0_zlp)
        {
            load_control_packet();
            R8_U2EP0_TX_CTRL ^= USBHS_UEP_T_TOG_DATA1;
        }
        else
        {
            ep0_phase = EP0_STATUS_OUT;
            R16_U2EP0_T_LEN = 0;
            R8_U2EP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_ACK;
            R8_U2EP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
        }
    }
    else if(ep0_phase == EP0_STATUS_IN)
    {
        if(request_type == 0x00 && request == USB_SET_ADDRESS) R8_USB2_DEV_AD = value;
        else if(request_type == 0x00 && request == USB_SET_CONFIGURATION)
        {
            configuration = value;
            remote_wakeup = wake_requested = wake_sent = suspend_observed = 0;
            ++session;
            reset_endpoint();
        }
        else if(request_type == 0x00 &&
                (request == USB_SET_FEATURE || request == USB_CLEAR_FEATURE))
        {
            remote_wakeup = request == USB_SET_FEATURE;
            if(!remote_wakeup) wake_requested = 0;
        }
        else if((request_type == 0x01 && request == USB_SET_INTERFACE) ||
                (request_type == 0x02 && request == USB_CLEAR_FEATURE))
            reset_endpoint();
        else if(request_type == 0x02 && request == USB_SET_FEATURE)
            R8_U2EP1_TX_CTRL = USBHS_UEP_T_RES_STALL;
        else if(request_type == 0x21 && request == HID_SET_IDLE) idle_rate = value >> 8;
        else if(request_type == 0x21 && request == HID_SET_PROTOCOL) protocol = value;
        ep0_phase = EP0_IDLE;
        R16_U2EP0_T_LEN = 0;
        R8_U2EP0_RX_CTRL = USBHS_UEP_R_RES_ACK;
        R8_U2EP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
    }
}

__INTERRUPT
__HIGH_CODE
void USB2_DEVICE_IRQHandler(void)
{
    uint8_t flags = R8_USB2_INT_FG;
    if(flags & USBHS_UDIF_BUS_RST)
    {
        configuration = 0;
        remote_wakeup = wake_requested = wake_sent = suspend_observed = 0;
        idle_rate = 0;
        protocol = 1;
        leds = 0;
        ++session;
        ep0_phase = EP0_IDLE;
        R8_USB2_DEV_AD = 0;
        R16_U2EP0_T_LEN = 0;
        R8_U2EP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
        R8_U2EP0_RX_CTRL = USBHS_UEP_R_RES_ACK;
        reset_endpoint();
        R8_USB2_INT_FG = flags & ~USBHS_UDIF_TRANSFER;
        return;
    }
    if(flags & USBHS_UDIF_TRANSFER)
    {
        uint8_t status = R8_USB2_INT_ST;
        uint8_t endpoint = status & USBHS_UDIS_EP_ID_MASK;
        if(status & USBHS_UDIS_EP_DIR)
        {
            if(endpoint == 0)
            {
                control_in_complete();
                R8_U2EP0_TX_CTRL &= ~USBHS_UEP_T_DONE;
            }
            else if(endpoint == 1)
            {
                R8_U2EP1_TX_CTRL = ((R8_U2EP1_TX_CTRL ^ USBHS_UEP_T_TOG_DATA1) &
                                    ~(USBHS_UEP_T_RES_MASK | USBHS_UEP_T_DONE)) | USBHS_UEP_T_RES_NAK;
            }
        }
        else if(endpoint == 0)
        {
            uint8_t rx = R8_U2EP0_RX_CTRL;
            if(rx & USBHS_UEP_R_SETUP_IS)
            {
                if(R16_U2EP0_RX_LEN == 8) setup_request();
                else stall();
            }
            else if(rx & USBHS_UEP_R_TOG_MATCH)
            {
                if(ep0_phase == EP0_DATA_OUT && R16_U2EP0_RX_LEN == 1)
                    {leds = ep0_buffer[0] & 0x1f; control_status();}
                else if(R16_U2EP0_RX_LEN == 0 &&
                        (ep0_phase == EP0_STATUS_OUT || ep0_phase == EP0_DATA_IN))
                {
                    ep0_phase = EP0_IDLE;
                    R8_U2EP0_RX_CTRL = USBHS_UEP_R_RES_ACK;
                    R8_U2EP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
                }
                else stall();
            }
            /* USBHS transfer flags are cleared by endpoint DONE, not INT_FG. */
            R8_U2EP0_RX_CTRL &= ~USBHS_UEP_R_DONE;
        }
    }
    /* LINK_RDY/suspend are not a bus reset. Keep the host's configuration
     * and endpoint toggle across idle/resume; the host need not enumerate
     * again. Poll the live state so a missed resume IRQ cannot latch sleep. */
    if((flags & USBHS_UDIF_LINK_RDY) && configuration &&
       !(R8_USB2_MIS_ST & (USBHS_UDMS_READY | USBHS_UDMS_SUSPEND | USBHS_UDMS_SLEEP)))
    {
        configuration = 0;
        remote_wakeup = wake_requested = wake_sent = suspend_observed = 0;
        ++session;
        reset_endpoint();
    }
    R8_USB2_INT_FG = flags & ~USBHS_UDIF_TRANSFER;
}

void UsbKeyboard_Init(void)
{
    PFIC_DisableIRQ(USB2_DEVICE_IRQn);
    configuration = 0;
    remote_wakeup = wake_requested = wake_sent = suspend_observed = 0;
    idle_rate = 0;
    protocol = 1;
    session = 0;
    ep0_phase = EP0_IDLE;
    GPIOB_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13, GPIO_ModeIN_Floating);
    /* WCH USBHS clock/pin initialization. Keep the BLE/NFC system clock. */
    sys_safe_access_enable();
    R16_CLK_SYS_CFG |= (RB_CLK_SYS_MOD & 0x40) | RB_XROM_SCLK_SEL | RB_OSC32M_SEL;
    sys_safe_access_disable();
    R8_USBHS_PLL_CTRL = USBHS_PLL_EN;
    R16_PIN_CONFIG |= RB_PIN_USB2_EN;
    R8_USB2_CTRL = USBHS_UD_RST_LINK | USBHS_UD_PHY_SUSPENDM;
    /* USBHS controller on PB12/PB13, forced to 12 Mbps for a boot keyboard.
     * This avoids high-speed descriptors and their different polling units. */
    R8_USB2_BASE_MODE = USBHS_UD_SPEED_FULL;
    R16_U2EP_TX_EN = RB_EP0_EN | RB_EP1_EN;
    R16_U2EP_RX_EN = RB_EP0_EN;
    R16_U2EP_T_TOG_AUTO = 0;
    R16_U2EP_R_TOG_AUTO = 0;
    R32_U2EP0_MAX_LEN = sizeof(ep0_buffer);
    R32_U2EP1_MAX_LEN = 8;
    R32_U2EP0_DMA = (uint32_t)ep0_buffer;
    R32_U2EP1_TX_DMA = (uint32_t)ep1_buffer;
    R8_U2EP0_RX_CTRL = USBHS_UEP_R_RES_ACK;
    R8_U2EP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
    reset_endpoint();
    R8_USB2_DEV_AD = 0;
    R8_USB2_INT_FG = 0xff;
    R8_USB2_INT_EN = USBHS_UDIE_TRANSFER | USBHS_UDIE_BUS_RST | USBHS_UDIE_SUSPEND |
                     USBHS_UDIE_BUS_SLEEP | USBHS_UDIE_LINK_RDY | USBHS_UDIE_FIFO_OVER;
    R8_USB2_CTRL = USBHS_UD_DEV_EN | USBHS_UD_DMA_EN | USBHS_UD_PHY_SUSPENDM;
    PFIC_EnableIRQ(USB2_DEVICE_IRQn);
}

uint8_t UsbKeyboard_Ready(void)
{
    uint8_t state = R8_USB2_MIS_ST;
    return configuration == 1 && (state & USBHS_UDMS_READY) &&
           !(state & (USBHS_UDMS_SUSPEND | USBHS_UDMS_SLEEP));
}
uint8_t UsbKeyboard_Connected(void)
{
    /* An enumerated sleeping host still owns the keyboard. READY can drop
     * during suspend, so it must not force a route change or lose key taps. */
    return configuration == 1 &&
           (R8_USB2_MIS_ST & (USBHS_UDMS_READY | USBHS_UDMS_SUSPEND | USBHS_UDMS_SLEEP));
}
void UsbKeyboard_RequestWakeup(void)
{
    if(UsbKeyboard_Connected() && !UsbKeyboard_Ready()) wake_requested = 1;
}
uint32_t UsbKeyboard_Session(void) {return session;}

uint8_t UsbKeyboard_Send(const uint8_t report[8])
{
    uint8_t accepted = 0;
    PFIC_DisableIRQ(USB2_DEVICE_IRQn);
    if(UsbKeyboard_Ready() && (R8_U2EP1_TX_CTRL & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_NAK)
    {
        memcpy(ep1_buffer, report, 8);
        ep1_buffer[1] = 0;
        memcpy(current_report, ep1_buffer, 8);
        R16_U2EP1_T_LEN = 8;
        R8_U2EP1_TX_CTRL = (R8_U2EP1_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_ACK;
        accepted = 1;
    }
    PFIC_EnableIRQ(USB2_DEVICE_IRQn);
    return accepted;
}

void UsbKeyboard_Poll(uint32_t now, uint8_t allow_idle_report)
{
    PFIC_DisableIRQ(USB2_DEVICE_IRQn);
    /* A confirmed inactive link needs fresh enumeration. A charger must
     * not inherit the PC's configuration on a battery-powered board. */
    if(configuration &&
       !(R8_USB2_MIS_ST & (USBHS_UDMS_READY | USBHS_UDMS_SUSPEND | USBHS_UDMS_SLEEP)))
    {
        configuration = 0;
        remote_wakeup = 0;
        ++session;
        reset_endpoint();
    }
    if(UsbKeyboard_Connected() &&
       (R8_USB2_MIS_ST & (USBHS_UDMS_SUSPEND | USBHS_UDMS_SLEEP)))
    {
        if(!suspend_observed)
        {
            suspend_observed = 1;
            suspend_time = now;
        }
        /* USB 2.0: observe at least 5 ms of idle before remote wakeup.
         * CH585 times and clears the resume signal in hardware (WCH SDK).
         * Send only once per suspend, and only when enabled by the host. */
        if(remote_wakeup && wake_requested && !wake_sent &&
           (uint32_t)(now - suspend_time) >= MS1_TO_SYSTEM_TIME(5))
        {
            R8_USB2_WAKE_CTRL |= USBHS_UD_UD_REMOTE_WKUP;
            wake_sent = 1;
        }
    }
    else
        suspend_observed = wake_requested = wake_sent = 0;
    PFIC_EnableIRQ(USB2_DEVICE_IRQn);
    if(!UsbKeyboard_Ready()) {last_report_time = now; return;}
    if(!allow_idle_report) {last_report_time = now; return;}
    if(idle_rate && (uint32_t)(now - last_report_time) >= MS1_TO_SYSTEM_TIME((uint32_t)idle_rate * 4))
    {
        if(UsbKeyboard_Send(current_report)) last_report_time = now;
    }
    else if((R8_U2EP1_TX_CTRL & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_ACK)
        last_report_time = now;
}
