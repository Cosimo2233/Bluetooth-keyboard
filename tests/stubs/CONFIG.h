#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H
#include <stdint.h>
#include <string.h>
/* Use the real chip's bit definitions, replace only its MMIO registers. */
#include "CH585SFR.h"
#define __INTERRUPT
#define __HIGH_CODE
#define SUCCESS 0
#define MS1_TO_SYSTEM_TIME(ms) ((uint32_t)(ms) * 8 / 5)
#define GPIO_Pin_12 (1u << 12)
#define GPIO_Pin_13 (1u << 13)
#define GPIO_ModeIN_Floating 0
#define GPIOB_ModeCfg(pins, mode) ((void)(pins), (void)(mode))
#define PFIC_DisableIRQ(irq) ((void)(irq))
#define PFIC_EnableIRQ(irq) ((void)(irq))
uint32_t TMOS_GetSystemClock(void);

#define sys_safe_access_enable() ((void)0)
#define sys_safe_access_disable() ((void)0)
#undef R16_CLK_SYS_CFG
extern uint16_t mock_R16_CLK_SYS_CFG;
#define R16_CLK_SYS_CFG mock_R16_CLK_SYS_CFG
#undef R16_PIN_CONFIG
extern uint16_t mock_R16_PIN_CONFIG;
#define R16_PIN_CONFIG mock_R16_PIN_CONFIG
#undef R16_U2EP_R_TOG_AUTO
extern uint16_t mock_R16_U2EP_R_TOG_AUTO;
#define R16_U2EP_R_TOG_AUTO mock_R16_U2EP_R_TOG_AUTO
#undef R16_U2EP_RX_EN
extern uint16_t mock_R16_U2EP_RX_EN;
#define R16_U2EP_RX_EN mock_R16_U2EP_RX_EN
#undef R16_U2EP_T_TOG_AUTO
extern uint16_t mock_R16_U2EP_T_TOG_AUTO;
#define R16_U2EP_T_TOG_AUTO mock_R16_U2EP_T_TOG_AUTO
#undef R16_U2EP_TX_EN
extern uint16_t mock_R16_U2EP_TX_EN;
#define R16_U2EP_TX_EN mock_R16_U2EP_TX_EN
#undef R16_U2EP0_RX_LEN
extern uint16_t mock_R16_U2EP0_RX_LEN;
#define R16_U2EP0_RX_LEN mock_R16_U2EP0_RX_LEN
#undef R16_U2EP0_T_LEN
extern uint16_t mock_R16_U2EP0_T_LEN;
#define R16_U2EP0_T_LEN mock_R16_U2EP0_T_LEN
#undef R16_U2EP1_T_LEN
extern uint16_t mock_R16_U2EP1_T_LEN;
#define R16_U2EP1_T_LEN mock_R16_U2EP1_T_LEN
#undef R32_U2EP0_DMA
extern uint32_t mock_R32_U2EP0_DMA;
#define R32_U2EP0_DMA mock_R32_U2EP0_DMA
#undef R32_U2EP0_MAX_LEN
extern uint32_t mock_R32_U2EP0_MAX_LEN;
#define R32_U2EP0_MAX_LEN mock_R32_U2EP0_MAX_LEN
#undef R32_U2EP1_MAX_LEN
extern uint32_t mock_R32_U2EP1_MAX_LEN;
#define R32_U2EP1_MAX_LEN mock_R32_U2EP1_MAX_LEN
#undef R32_U2EP1_TX_DMA
extern uint32_t mock_R32_U2EP1_TX_DMA;
#define R32_U2EP1_TX_DMA mock_R32_U2EP1_TX_DMA
#undef R8_U2EP0_RX_CTRL
extern uint8_t mock_R8_U2EP0_RX_CTRL;
#define R8_U2EP0_RX_CTRL mock_R8_U2EP0_RX_CTRL
#undef R8_U2EP0_TX_CTRL
extern uint8_t mock_R8_U2EP0_TX_CTRL;
#define R8_U2EP0_TX_CTRL mock_R8_U2EP0_TX_CTRL
#undef R8_U2EP1_TX_CTRL
extern uint8_t mock_R8_U2EP1_TX_CTRL;
#define R8_U2EP1_TX_CTRL mock_R8_U2EP1_TX_CTRL
#undef R8_USB2_BASE_MODE
extern uint8_t mock_R8_USB2_BASE_MODE;
#define R8_USB2_BASE_MODE mock_R8_USB2_BASE_MODE
#undef R8_USB2_CTRL
extern uint8_t mock_R8_USB2_CTRL;
#define R8_USB2_CTRL mock_R8_USB2_CTRL
#undef R8_USB2_DEV_AD
extern uint8_t mock_R8_USB2_DEV_AD;
#define R8_USB2_DEV_AD mock_R8_USB2_DEV_AD
#undef R8_USB2_INT_EN
extern uint8_t mock_R8_USB2_INT_EN;
#define R8_USB2_INT_EN mock_R8_USB2_INT_EN
#undef R8_USB2_INT_FG
extern uint8_t mock_R8_USB2_INT_FG;
#define R8_USB2_INT_FG mock_R8_USB2_INT_FG
#undef R8_USB2_INT_ST
extern uint8_t mock_R8_USB2_INT_ST;
#define R8_USB2_INT_ST mock_R8_USB2_INT_ST
#undef R8_USB2_MIS_ST
extern uint8_t mock_R8_USB2_MIS_ST;
#define R8_USB2_MIS_ST mock_R8_USB2_MIS_ST
#undef R8_USBHS_PLL_CTRL
extern uint8_t mock_R8_USBHS_PLL_CTRL;
#define R8_USBHS_PLL_CTRL mock_R8_USBHS_PLL_CTRL
#endif
