#include <stdint.h>
#define HID_REPORT_TYPE_INPUT 1
uint8_t HidDev_Report(uint8_t id, uint8_t type, uint8_t len, uint8_t *data);
