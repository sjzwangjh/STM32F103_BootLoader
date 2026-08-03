/**
 * @file    Stk500Protocol.h
 * @brief   Shared STK500v2 byte-stream state machine for USB transports.
 */
#ifndef __STK500PROTOCOL_H_INCLUDED__
#define __STK500PROTOCOL_H_INCLUDED__

#include <stdint.h>
#include <stddef.h>
#include "sys.h"

#define STK_STX     0x1BU
#define STK_TOKEN   0x0EU

#define STK_DATA_SOURCE_USB_HID         0U
#define STK_DATA_SOURCE_USB_CDC         1U
#define STK_DATA_SOURCE_USB_WINUSB      2U

#define BUFFER_SIZE     281U
#define RX_TIMEOUT      200U

void     stkSetRxChar(uint8_t source, uint8_t data);
void     stkFeedBytes(uint8_t source, const uint8_t *data, uint16_t len);
uint16_t stkPoll(void);
uint16_t stkGetTxCount(uint8_t source);
int      stkGetTxByte(uint8_t source);
uint32_t stkGetFlashAddress(void);
uint8_t  stkShouldEnterBootloader(void);
uint8_t  stkTryStartApplication(void);
void     stkService(void);
void     stkWinUSBTask(void);
void     stkWinUSBFlush(void);

#endif /* __STK500PROTOCOL_H_INCLUDED__ */
