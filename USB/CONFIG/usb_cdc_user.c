/*
 * USB CDC user layer - forward raw bytes to the shared STK500 state machine.
 */

#include "sys.h"
#include "usb_lib.h"
#include "usb_conf.h"
#include "usb_regs.h"
#include "usb_cdc_user.h"
#include "Stk500Protocol.h"
#include "Hardware_Config.h"
#include "usart.h"
#include <string.h>

static uint8_t cdcRxPacket[CDC_RX_PACKET_SIZE];
static uint8_t cdcTxPacket[CDC_TX_PACKET_SIZE];
static uint8_t cdcTxBusy;

static uint8_t  cdcRingBuf[CDC_RX_RING_SIZE];
static uint16_t cdcRingHead;
static uint16_t cdcRingTail;
static uint8_t  cdcRxPending;

static usb_cdc_line_coding_t cdcLineCoding = {
    115200U,
    0U,
    0U,
    8U
};
static uint8_t cdcLineCodingBuf[7];
static uint16_t cdcControlLineState;

#if IS_DEBUG_USB
static void CdcDebugWriteDec(uint16_t value)
{
    char buf[6];
    uint8_t i = 0U;
    if (value == 0U)
    {
        uart1_WriteByte('0');
        return;
    }
    while (value > 0U && i < sizeof(buf))
    {
        buf[i++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (i > 0U)
    {
        uart1_WriteByte((uint8_t)buf[--i]);
    }
}
#endif

static void ringBufWrite(const uint8_t *data, uint16_t len);
static uint8_t ringBufRead(uint8_t *byte);
static void CDC_StartNextTxPacket(void);

void CDC_Init(void)
{
    cdcTxBusy = 0U;
    cdcControlLineState = 0U;
    cdcRxPending = 0U;
    cdcRingHead = 0U;
    cdcRingTail = 0U;
    CDC_FillLineCodingBuffer();
}

uint8_t CDC_SendData(const uint8_t *data, uint16_t len)
{
    uint16_t sendLen;

    if (data == 0 || len == 0U)
        return 0U;

    if (cdcTxBusy != 0U)
        return 1U;

    sendLen = len;
    if (sendLen > CDC_TX_PACKET_SIZE)
        sendLen = CDC_TX_PACKET_SIZE;

    memcpy(cdcTxPacket, data, sendLen);
    UserToPMABufferCopy(cdcTxPacket, ENDP3_TXADDR, sendLen);
    SetEPTxCount(ENDP3, sendLen);
    cdcTxBusy = 1U;
    SetEPTxValid(ENDP3);
    return 0U;
}

uint8_t CDC_IsTxBusy(void)
{
    return cdcTxBusy;
}

void CDC_DataIn_Callback(void)
{
    cdcTxBusy = 0U;
}

void CDC_DataOut_Callback(void)
{
    uint16_t rxLen;

    rxLen = GetEPRxCount(ENDP3);
    if (rxLen > CDC_RX_PACKET_SIZE)
        rxLen = CDC_RX_PACKET_SIZE;

    PMAToUserBufferCopy(cdcRxPacket, ENDP3_RXADDR, rxLen);
    SetEPRxValid(ENDP3);

    ringBufWrite(cdcRxPacket, rxLen);
    cdcRxPending = 1U;
}

void CDC_Task(void)
{
    uint8_t byte;

    if (cdcRxPending)
    {
        cdcRxPending = 0U;
        while (ringBufRead(&byte))
            stkSetRxChar(STK_DATA_SOURCE_USB_CDC, byte);
    }

    if (stkGetTxCount(STK_DATA_SOURCE_USB_CDC) != 0U && !cdcTxBusy)
        CDC_StartNextTxPacket();
}

uint8_t *CDC_GetLineCodingBuffer(void)
{
    return cdcLineCodingBuf;
}

void CDC_FillLineCodingBuffer(void)
{
    cdcLineCodingBuf[0] = (uint8_t)(cdcLineCoding.bitrate);
    cdcLineCodingBuf[1] = (uint8_t)(cdcLineCoding.bitrate >> 8);
    cdcLineCodingBuf[2] = (uint8_t)(cdcLineCoding.bitrate >> 16);
    cdcLineCodingBuf[3] = (uint8_t)(cdcLineCoding.bitrate >> 24);
    cdcLineCodingBuf[4] = cdcLineCoding.format;
    cdcLineCodingBuf[5] = cdcLineCoding.paritytype;
    cdcLineCodingBuf[6] = cdcLineCoding.datatype;
}

void CDC_SetLineCodingFromBuffer(void)
{
    cdcLineCoding.bitrate = ((uint32_t)cdcLineCodingBuf[0]) |
                            ((uint32_t)cdcLineCodingBuf[1] << 8) |
                            ((uint32_t)cdcLineCodingBuf[2] << 16) |
                            ((uint32_t)cdcLineCodingBuf[3] << 24);
    cdcLineCoding.format = cdcLineCodingBuf[4];
    cdcLineCoding.paritytype = cdcLineCodingBuf[5];
    cdcLineCoding.datatype = cdcLineCodingBuf[6];
}

void CDC_SetControlLineState(uint16_t state)
{
    cdcControlLineState = state;
}

uint16_t CDC_GetControlLineState(void)
{
    return cdcControlLineState;
}

static void ringBufWrite(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    for (i = 0U; i < len; i++)
    {
        uint16_t next = (uint16_t)(cdcRingHead + 1U) % CDC_RX_RING_SIZE;
        if (next != cdcRingTail)
        {
            cdcRingBuf[cdcRingHead] = data[i];
            cdcRingHead = next;
        }
    }
}

static uint8_t ringBufRead(uint8_t *byte)
{
    if (cdcRingHead == cdcRingTail)
        return 0U;

    *byte = cdcRingBuf[cdcRingTail];
    cdcRingTail = (uint16_t)(cdcRingTail + 1U) % CDC_RX_RING_SIZE;
    return 1U;
}

static void CDC_StartNextTxPacket(void)
{
    uint16_t chunk;
    uint16_t i;
    int c;

    if (cdcTxBusy != 0U)
        return;

    chunk = stkGetTxCount(STK_DATA_SOURCE_USB_CDC);
    if (chunk == 0U)
        return;
    if (chunk > CDC_TX_PACKET_SIZE)
        chunk = CDC_TX_PACKET_SIZE;

    for (i = 0U; i < chunk; i++)
    {
        c = stkGetTxByte(STK_DATA_SOURCE_USB_CDC);
        if (c < 0)
            break;
        cdcTxPacket[i] = (uint8_t)c;
    }

    if (i > 0U)
        (void)CDC_SendData(cdcTxPacket, i);
}
