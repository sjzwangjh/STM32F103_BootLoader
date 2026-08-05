/*
 * ????: usb_winusb_user.c
 * ????: USB ?? / USB ????
 * ????: ???
 * ????: ??????? Bootloader ??????????????
 * ????: ????????????????????????????? GB2312 ???
 */
/*
 * USB WinUSB Bulk user-layer implementation
 */

#include "sys.h"
#include "usb_lib.h"
#include "usb_conf.h"
#include "usb_regs.h"
#include "usb_winusb_user.h"
#include "Stk500Protocol.h"
#include "usart.h"

#define WINUSB_RX_BUF_SIZE  2048
#define IS_DEBUG_APP        1U

static u8   g_winusb_rx_buf[WINUSB_RX_BUF_SIZE];
static u16  g_winusb_rx_head = 0;
static u16  g_winusb_rx_tail = 0;
static u8   g_winusb_tx_busy = 0;
static u8   g_winusb_tx_pkt[64];

static void WinUSB_StartNextTxPacket(void);

#if IS_DEBUG_APP
static void WinUsbDebugWriteDec(u16 value)
{
    char buf[6];
    u8 i = 0U;
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
        uart1_WriteByte((u8)buf[--i]);
    }
}
#endif

static void WinUSB_TxPollDone(void)
{
    if (g_winusb_tx_busy == 0U)
    {
        return;
    }
    if ((_GetENDPOINT(ENDP4) & EP_CTR_TX) != 0U)
    {
        ClearEP_CTR_TX(ENDP4);
        g_winusb_tx_busy = 0U;
    }
    else if (_GetEPTxStatus(ENDP4) == EP_TX_NAK)
    {
        /* The STM32 USB hardware returns the endpoint to NAK after a completed
         * IN transfer. If the IN callback was missed (e.g. the host cancelled a
         * read while the packet was in flight), the busy flag would otherwise
         * stay set forever and block every later response. Self-heal it here. */
        g_winusb_tx_busy = 0U;
    }
}

/* EP4 IN callback - transfer complete */
void WinUSB_IN_Callback(void)
{
    g_winusb_tx_busy = 0U;
}

/* EP4 OUT callback - data arrived */
void WinUSB_OUT_Callback(void)
{
    u16 rx_count = GetEPRxCount(ENDP4);
    u8  temp[64];
    u16 i;

    if (rx_count > 0U && rx_count <= 64U)
    {
        PMAToUserBufferCopy(temp, ENDP4_RXADDR, rx_count);

        for (i = 0U; i < rx_count; i++)
        {
            u16 next = (u16)((g_winusb_rx_head + 1U) % WINUSB_RX_BUF_SIZE);
            if (next != g_winusb_rx_tail)
            {
                g_winusb_rx_buf[g_winusb_rx_head] = temp[i];
                g_winusb_rx_head = next;
            }
        }
    }

    SetEPRxStatus(ENDP4, EP_RX_VALID);
}

/* Send data on EP4 IN bulk endpoint. Returns 0 on queued, 1 if busy/invalid. */
static u8 WinUSB_Bulk_Send(const u8 *buf, u16 len)
{
    WinUSB_TxPollDone();

    if (buf == 0 || len == 0U || len > 64U)
        return 1U;
    if (g_winusb_tx_busy != 0U)
        return 1U;

    UserToPMABufferCopy((u8 *)buf, ENDP4_TXADDR, len);
    SetEPTxCount(ENDP4, len);
    g_winusb_tx_busy = 1U;
    SetEPTxStatus(ENDP4, EP_TX_VALID);
#if IS_DEBUG_APP
    uart1_WriteString("WINUSB TX len=");
    WinUsbDebugWriteDec(len);
    uart1_WriteString("\r\n");
#endif
    return 0U;
}

/* Receive data from ring buffer */
static u16 WinUSB_Bulk_Recv(u8 *buf, u16 maxLen)
{
    u16 i;

    if (buf == 0)
        return 0U;

    for (i = 0U; i < maxLen && g_winusb_rx_head != g_winusb_rx_tail; i++)
    {
        buf[i] = g_winusb_rx_buf[g_winusb_rx_tail];
        g_winusb_rx_tail = (u16)((g_winusb_rx_tail + 1U) % WINUSB_RX_BUF_SIZE);
    }
#if IS_DEBUG_APP
    if (i > 0U)
    {
        uart1_WriteString("WINUSB RX len=");
        WinUsbDebugWriteDec(i);
        uart1_WriteString("\r\n");
    }
#endif
    return i;
}

/* Query available byte count */
static u16 WinUSB_Bulk_Available(void)
{
    /* SPSC ring: head is written only by the USB ISR, tail only by the main
     * loop. Never keep a shared counter here - a read-modify-write on a shared
     * byte count from both contexts loses updates and stalls frame assembly. */
    return (u16)((g_winusb_rx_head - g_winusb_rx_tail) & (WINUSB_RX_BUF_SIZE - 1U));
}

/* Query TX busy state, with a polling fallback in case the IN callback is missed. */
static u8 WinUSB_Bulk_TxBusy(void)
{
    WinUSB_TxPollDone();
    return g_winusb_tx_busy;
}

/* ---- TX: push pending STK response via EP4 IN (main-loop context) ---- */
static void WinUSB_StartNextTxPacket(void)
{
    u16 chunk;
    u16 i;
    int c;

    if (WinUSB_Bulk_TxBusy()) return;

    chunk = stkGetTxCount(STK_DATA_SOURCE_USB_WINUSB);
    if (chunk == 0U) return;
    if (chunk > 64U) chunk = 64U;

    for (i = 0U; i < chunk; i++)
    {
        c = stkGetTxByte(STK_DATA_SOURCE_USB_WINUSB);
        if (c < 0) break;
        g_winusb_tx_pkt[i] = (u8)c;
    }

    if (i > 0U)
        (void)WinUSB_Bulk_Send(g_winusb_tx_pkt, i);
}

/* ---- Single main-loop entry: drain RX ring + flush TX (EP4) ---- */
void WinUSB_Task(void)
{
    u8  buf[256];
    u16 n, i;

    n = WinUSB_Bulk_Available();
    if (n > 0U)
    {
        if (n > sizeof(buf)) n = sizeof(buf);
        n = WinUSB_Bulk_Recv(buf, n);
        for (i = 0U; i < n; i++)
            stkSetRxChar(STK_DATA_SOURCE_USB_WINUSB, buf[i]);
    }

    if (stkGetTxCount(STK_DATA_SOURCE_USB_WINUSB) != 0U)
        WinUSB_StartNextTxPacket();
}
