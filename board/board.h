/*
 * Copyright 2020, 2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _BOARD_H_
#define _BOARD_H_

#include "clock_config.h"
#include "fsl_common.h"
#include "fsl_gpio.h"
#include "fsl_clock.h"
#if defined(CONFIG_BT_LOW_POWER_MODE) && (CONFIG_BT_LOW_POWER_MODE)
#if (defined(BUTTON_COUNT) && (BUTTON_COUNT > 0U))
#include "fsl_component_button.h"
#endif /* BUTTON_COUNT */
#endif /* CONFIG_BT_LOW_POWER_MODE */

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*! @brief The board name */
#define BOARD_NAME "MIMXRT1170-EVK"
#ifndef DEBUG_CONSOLE_UART_INDEX
#define DEBUG_CONSOLE_UART_INDEX 1
#endif

/* The UART to use for debug messages. */
#define BOARD_DEBUG_UART_TYPE     kSerialPort_Uart
#define BOARD_DEBUG_UART_CLK_FREQ 24000000

#if DEBUG_CONSOLE_UART_INDEX == 1
#define BOARD_DEBUG_UART_BASEADDR (uint32_t) LPUART1
#define BOARD_DEBUG_UART_INSTANCE 1U
#define BOARD_UART_IRQ            LPUART1_IRQn
#define BOARD_UART_IRQ_HANDLER    LPUART1_IRQHandler
#elif DEBUG_CONSOLE_UART_INDEX == 2
#define BOARD_DEBUG_UART_BASEADDR (uint32_t) LPUART2
#define BOARD_DEBUG_UART_INSTANCE 2U
#define BOARD_UART_IRQ            LPUART2_IRQn
#define BOARD_UART_IRQ_HANDLER    LPUART2_IRQHandler
#elif DEBUG_CONSOLE_UART_INDEX == 12
#define BOARD_DEBUG_UART_BASEADDR (uint32_t) LPUART12
#define BOARD_DEBUG_UART_INSTANCE 12U
#define BOARD_UART_IRQ            LPUART12_IRQn
#define BOARD_UART_IRQ_HANDLER    LPUART12_IRQHandler
#else
#error "Unsupported UART"
#endif

#ifndef BOARD_DEBUG_UART_BAUDRATE
#define BOARD_DEBUG_UART_BAUDRATE (115200U)
#endif /* BOARD_DEBUG_UART_BAUDRATE */

/* Definitions for eRPC MU transport layer */
#if defined(FSL_FEATURE_MU_SIDE_A)
#define MU_BASE        MUA
#define MU_IRQ         MUA_IRQn
#define MU_IRQ_HANDLER MUA_IRQHandler
#endif
#if defined(FSL_FEATURE_MU_SIDE_B)
#define MU_BASE        MUB
#define MU_IRQ         MUB_IRQn
#define MU_IRQ_HANDLER MUB_IRQHandler
#endif
#define MU_IRQ_PRIORITY (2)

/*! @brief The USER_LED used for board */
#define LOGIC_LED_ON  (0U)
#define LOGIC_LED_OFF (1U)
#ifndef BOARD_USER_LED_GPIO
#define BOARD_USER_LED_GPIO GPIO9
#endif
#ifndef BOARD_USER_LED_GPIO_PIN
#define BOARD_USER_LED_GPIO_PIN (3U)
#endif

#define USER_LED_INIT(output)                                            \
    GPIO_PinWrite(BOARD_USER_LED_GPIO, BOARD_USER_LED_GPIO_PIN, output); \
    BOARD_USER_LED_GPIO->GDIR |= (1U << BOARD_USER_LED_GPIO_PIN)                       /*!< Enable target USER_LED */
#define USER_LED_OFF() \
    GPIO_PortClear(BOARD_USER_LED_GPIO, 1U << BOARD_USER_LED_GPIO_PIN)                 /*!< Turn off target USER_LED */
#define USER_LED_ON() GPIO_PortSet(BOARD_USER_LED_GPIO, 1U << BOARD_USER_LED_GPIO_PIN) /*!<Turn on target USER_LED*/
#define USER_LED_TOGGLE()                                       \
    GPIO_PinWrite(BOARD_USER_LED_GPIO, BOARD_USER_LED_GPIO_PIN, \
                  0x1 ^ GPIO_PinRead(BOARD_USER_LED_GPIO, BOARD_USER_LED_GPIO_PIN)) /*!< Toggle target USER_LED */

/*! @brief Define the port interrupt number for the board switches */
#ifndef BOARD_USER_BUTTON_GPIO
#define BOARD_USER_BUTTON_GPIO GPIO13
#endif
#ifndef BOARD_USER_BUTTON_GPIO_PIN
#define BOARD_USER_BUTTON_GPIO_PIN (0U)
#endif
#define BOARD_USER_BUTTON_IRQ         GPIO13_Combined_0_31_IRQn
#define BOARD_USER_BUTTON_IRQ_HANDLER GPIO13_Combined_0_31_IRQHandler
#define BOARD_USER_BUTTON_NAME        "SW7"

#if defined(CONFIG_BT_LOW_POWER_MODE) && (CONFIG_BT_LOW_POWER_MODE)
#ifndef BUTTON_COUNT
#define BUTTON_COUNT 1
#endif
#if (defined(BUTTON_COUNT) && (BUTTON_COUNT > 0U))
#define APP_WAKEUP_BUTTON               BOARD_USER_BUTTON_GPIO
#define APP_WAKEUP_BUTTON_GPIO          13
#define APP_WAKEUP_BUTTON_GPIO_PIN      BOARD_USER_BUTTON_GPIO_PIN
#define APP_WAKEUP_BUTTON_DEFAULT_VALUE 1
#define APP_WAKEUP_BUTTON_IRQ           BOARD_USER_BUTTON_IRQ
#define APP_WAKEUP_BUTTON_IRQ_HANDLER   BOARD_USER_BUTTON_IRQ_HANDLER
#define APP_WAKEUP_BUTTON_NAME          BOARD_USER_BUTTON_NAME

extern button_config_t g_buttonConfig[];
#endif /* BUTTON_COUNT */
#endif /* CONFIG_BT_LOW_POWER_MODE */

/*! @brief The board flash size */
#define BOARD_FLASH_SIZE (0x1000000U)

/* SKIP_SEMC_INIT can also be defined independently */
#ifdef USE_SDRAM
#define SKIP_SEMC_INIT
#endif

/*! @brief The ENET0 PHY address. */
#define BOARD_ENET0_PHY_ADDRESS (0x02U) /* Phy address of enet port 0. */

/*! @brief The ENET1 PHY address. */
#define BOARD_ENET1_PHY_ADDRESS (0x01U) /* Phy address of enet port 1. */

/*! @brief The ENET PHY used for board. */
#ifndef BOARD_ENET_PHY0_RESET_GPIO
#define BOARD_ENET_PHY0_RESET_GPIO GPIO12
#endif
#ifndef BOARD_ENET_PHY0_RESET_GPIO_PIN
#define BOARD_ENET_PHY0_RESET_GPIO_PIN (12U)
#endif

#define BOARD_ENET_PHY0_RESET                                                           \
    GPIO_WritePinOutput(BOARD_ENET_PHY0_RESET_GPIO, BOARD_ENET_PHY0_RESET_GPIO_PIN, 0); \
    SDK_DelayAtLeastUs(10000, CLOCK_GetFreq(kCLOCK_CpuClk));                            \
    GPIO_WritePinOutput(BOARD_ENET_PHY0_RESET_GPIO, BOARD_ENET_PHY0_RESET_GPIO_PIN, 1); \
    SDK_DelayAtLeastUs(100, CLOCK_GetFreq(kCLOCK_CpuClk))

#ifndef BOARD_ENET_PHY1_RESET_GPIO
#define BOARD_ENET_PHY1_RESET_GPIO GPIO11
#endif
#ifndef BOARD_ENET_PHY1_RESET_GPIO_PIN
#define BOARD_ENET_PHY1_RESET_GPIO_PIN (14U)
#endif

/* For a complete PHY reset of RTL8211FDI-CG, this pin must be asserted low for at least 10ms. And
 * wait for a further 30ms(for internal circuits settling time) before accessing the PHY register */
#define BOARD_ENET_PHY1_RESET                                                           \
    GPIO_WritePinOutput(BOARD_ENET_PHY1_RESET_GPIO, BOARD_ENET_PHY1_RESET_GPIO_PIN, 0); \
    SDK_DelayAtLeastUs(10000, CLOCK_GetFreq(kCLOCK_CpuClk));                            \
    GPIO_WritePinOutput(BOARD_ENET_PHY1_RESET_GPIO, BOARD_ENET_PHY1_RESET_GPIO_PIN, 1); \
    SDK_DelayAtLeastUs(30000, CLOCK_GetFreq(kCLOCK_CpuClk))

/*! @brief The EMVSIM SMARTCARD PHY configuration. */
#define BOARD_SMARTCARD_MODULE                (EMVSIM1)      /*!< SMARTCARD communicational module instance */
#define BOARD_SMARTCARD_MODULE_IRQ            (EMVSIM1_IRQn) /*!< SMARTCARD communicational module IRQ handler */
#define BOARD_SMARTCARD_CLOCK_MODULE_CLK_FREQ (CLOCK_GetRootClockFreq(kCLOCK_Root_Emv1))
#define BOARD_SMARTCARD_CLOCK_VALUE           (4000000U)     /*!< SMARTCARD clock frequency */

/* USB PHY condfiguration */
#define BOARD_USB_PHY_D_CAL     (0x07U)
#define BOARD_USB_PHY_TXCAL45DP (0x06U)
#define BOARD_USB_PHY_TXCAL45DM (0x06U)

#define BOARD_ARDUINO_INT_IRQ   (GPIO1_INT3_IRQn)
#define BOARD_ARDUINO_I2C_IRQ   (LPI2C1_IRQn)
#define BOARD_ARDUINO_I2C_INDEX (1)

#define BOARD_HAS_SDCARD (1U)

/* @Brief Board accelerator sensor configuration */
#define BOARD_ACCEL_I2C_BASEADDR LPI2C5
/* Clock divider for LPI2C clock source */
#define BOARD_ACCEL_I2C_CLOCK_FREQ (CLOCK_GetRootClockFreq(kCLOCK_Root_Lpi2c5))

#define BOARD_CODEC_I2C_BASEADDR             LPI2C5
#define BOARD_CODEC_I2C_INSTANCE             5U
#define BOARD_CODEC_I2C_CLOCK_SOURCE_SELECT  (0U)
#define BOARD_CODEC_I2C_CLOCK_SOURCE_DIVIDER (6U)
#define BOARD_CODEC_I2C_CLOCK_FREQ           (24000000U)

/* @Brief Board CAMERA configuration */
#define BOARD_CAMERA_I2C_BASEADDR      LPI2C6
#define BOARD_CAMERA_I2C_CLOCK_ROOT    kCLOCK_Root_Lpi2c6
#define BOARD_CAMERA_I2C_CLOCK_SOURCE  (1U)  /* OSC24M. */
#define BOARD_CAMERA_I2C_CLOCK_DIVIDER (12U) /* Divider = 12, LPI2C clock frequency 2M. */

/*! @brief The MIPI panel pins. */
#define BOARD_MIPI_PANEL_RST_GPIO   GPIO9
#define BOARD_MIPI_PANEL_RST_PIN    1
#define BOARD_MIPI_PANEL_POWER_GPIO GPIO11
#define BOARD_MIPI_PANEL_POWER_PIN  16
/* Back light pin. */
#define BOARD_MIPI_PANEL_BL_GPIO GPIO9
#define BOARD_MIPI_PANEL_BL_PIN  29

/* Touch panel. */
#define BOARD_MIPI_PANEL_TOUCH_I2C_BASEADDR      LPI2C5
#define BOARD_MIPI_PANEL_TOUCH_I2C_CLOCK_ROOT    kCLOCK_Root_Lpi2c5
#define BOARD_MIPI_PANEL_TOUCH_I2C_CLOCK_SOURCE  (1U)  /* OSC24M. */
#define BOARD_MIPI_PANEL_TOUCH_I2C_CLOCK_DIVIDER (12U) /* Divider = 12, LPI2C clock frequency 2M. */
#define BOARD_MIPI_PANEL_TOUCH_I2C_CLOCK_FREQ    CLOCK_GetRootClockFreq(BOARD_MIPI_PANEL_TOUCH_I2C_CLOCK_ROOT)
#define BOARD_MIPI_PANEL_TOUCH_RST_GPIO          GPIO9
#define BOARD_MIPI_PANEL_TOUCH_RST_PIN           0
#define BOARD_MIPI_PANEL_TOUCH_INT_GPIO          GPIO8
#define BOARD_MIPI_PANEL_TOUCH_INT_PIN           31

/*! @brief The camera pins. */
#define BOARD_CAMERA_PWDN_GPIO GPIO9
#define BOARD_CAMERA_PWDN_PIN  25
#define BOARD_CAMERA_RST_GPIO  GPIO11
#define BOARD_CAMERA_RST_PIN   15

/* SD card detection method when using wifi module. */
#define BOARD_WIFI_SD_DETECT_TYPE kSDMMCHOST_DetectCardByHostDATA3

#define BOARD_BT_UART_INSTANCE    7
#define BOARD_BT_UART_BASEADDR    (uint32_t) LPUART7
#define BOARD_BT_UART_BAUDRATE    3000000
#define BOARD_BT_UART_CLK_FREQ    CLOCK_GetRootClockFreq(kCLOCK_Root_Lpuart7);
#define BOARD_BT_UART_IRQ         LPUART7_IRQn
#define BOARD_BT_UART_IRQ_HANDLER LPUART7_IRQHandler

/*! @brief The Ethernet port used by network examples, default use 1G port. */
/* Below comment is for test script to easily define which port to be used, please don't delete. */
/* @TEST_ANCHOR */
#ifndef BOARD_NETWORK_USE_100M_ENET_PORT
#define BOARD_NETWORK_USE_100M_ENET_PORT (0U)
#endif

/* Timer Manager definition. */
#define BOARD_TM_INSTANCE   1
#define BOARD_TM_CLOCK_ROOT kCLOCK_Root_Gpt1

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
 * API
 ******************************************************************************/
uint32_t BOARD_DebugConsoleSrcFreq(void);

void BOARD_InitDebugConsole(void);

void BOARD_ConfigMPU(void);
#if defined(SDK_I2C_BASED_COMPONENT_USED) && SDK_I2C_BASED_COMPONENT_USED
void BOARD_LPI2C_Init(LPI2C_Type *base, uint32_t clkSrc_Hz);
status_t BOARD_LPI2C_Send(LPI2C_Type *base,
                          uint8_t deviceAddress,
                          uint32_t subAddress,
                          uint8_t subaddressSize,
                          uint8_t *txBuff,
                          uint8_t txBuffSize);
status_t BOARD_LPI2C_Receive(LPI2C_Type *base,
                             uint8_t deviceAddress,
                             uint32_t subAddress,
                             uint8_t subaddressSize,
                             uint8_t *rxBuff,
                             uint8_t rxBuffSize);
status_t BOARD_LPI2C_SendSCCB(LPI2C_Type *base,
                              uint8_t deviceAddress,
                              uint32_t subAddress,
                              uint8_t subaddressSize,
                              uint8_t *txBuff,
                              uint8_t txBuffSize);
status_t BOARD_LPI2C_ReceiveSCCB(LPI2C_Type *base,
                                 uint8_t deviceAddress,
                                 uint32_t subAddress,
                                 uint8_t subaddressSize,
                                 uint8_t *rxBuff,
                                 uint8_t rxBuffSize);
void BOARD_Accel_I2C_Init(void);
status_t BOARD_Accel_I2C_Send(uint8_t deviceAddress, uint32_t subAddress, uint8_t subaddressSize, uint32_t txBuff);
status_t BOARD_Accel_I2C_Receive(
    uint8_t deviceAddress, uint32_t subAddress, uint8_t subaddressSize, uint8_t *rxBuff, uint8_t rxBuffSize);
void BOARD_Codec_I2C_Init(void);
status_t BOARD_Codec_I2C_Send(
    uint8_t deviceAddress, uint32_t subAddress, uint8_t subAddressSize, const uint8_t *txBuff, uint8_t txBuffSize);
status_t BOARD_Codec_I2C_Receive(
    uint8_t deviceAddress, uint32_t subAddress, uint8_t subAddressSize, uint8_t *rxBuff, uint8_t rxBuffSize);
void BOARD_Camera_I2C_Init(void);
status_t BOARD_Camera_I2C_Send(
    uint8_t deviceAddress, uint32_t subAddress, uint8_t subAddressSize, const uint8_t *txBuff, uint8_t txBuffSize);
status_t BOARD_Camera_I2C_Receive(
    uint8_t deviceAddress, uint32_t subAddress, uint8_t subAddressSize, uint8_t *rxBuff, uint8_t rxBuffSize);

status_t BOARD_Camera_I2C_SendSCCB(
    uint8_t deviceAddress, uint32_t subAddress, uint8_t subAddressSize, const uint8_t *txBuff, uint8_t txBuffSize);
status_t BOARD_Camera_I2C_ReceiveSCCB(
    uint8_t deviceAddress, uint32_t subAddress, uint8_t subAddressSize, uint8_t *rxBuff, uint8_t rxBuffSize);

void BOARD_MIPIPanelTouch_I2C_Init(void);
status_t BOARD_MIPIPanelTouch_I2C_Send(
    uint8_t deviceAddress, uint32_t subAddress, uint8_t subAddressSize, const uint8_t *txBuff, uint8_t txBuffSize);
status_t BOARD_MIPIPanelTouch_I2C_Receive(
    uint8_t deviceAddress, uint32_t subAddress, uint8_t subAddressSize, uint8_t *rxBuff, uint8_t rxBuffSize);
#endif /* SDK_I2C_BASED_COMPONENT_USED */

void BOARD_SD_Pin_Config(uint32_t speed, uint32_t strength);
void BOARD_MMC_Pin_Config(uint32_t speed, uint32_t strength);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* _BOARD_H_ */
