/*
 * USB HID�û���ʵ�� - HID�����շ���Ӧ�ò�ӿ�?
 */

/*
 * USB HID Data Relay Layer - STK500 protocol integration
 * Ported from AVR-Doper firmware/main.c (C. Starkjohann, obdev.at)
 */

#include "sys.h"
#include "usb_hid_user.h"
#include "Stk500Protocol.h"
#include "Hardware_Config.h"
#include "usart.h"

uint8_t  g_HidReportId = 0;
RequestType_t g_RequestType = REQUEST_TYPE_IDLE;

#if IS_DEBUG_USB
static void HidDebugWriteDec(uint16_t value)
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

static uint16_t HID_GetPayloadSize(uint8_t reportId)
{
    switch (reportId)
    {
        case 1: return 14U;
        case 2: return 30U;
        case 3: return 62U;
        case 4: return 126U;
        default: return 0U;
    }
}

void HID_BeginReportRequest(uint8_t reportId, RequestType_t requestType)
{
    g_HidReportId = reportId;
    g_RequestType = requestType;
}

void HID_Rx_Store(uint8_t reportId, const uint8_t *data, uint8_t len)
{
    uint8_t i;
    uint8_t stkPayloadStart;
    uint8_t stkLen;

#if IS_DEBUG_USB
    uart1_WriteString("HID RX len=");
    HidDebugWriteDec(len);
    uart1_WriteString("\r\n");
#endif

    if (data == NULL || len == 0U)
    {
        HID_ResetRequestState();
        return;
    }

    /*
     * AVR-Doper hid_send_feature_report() ���͸�ʽ:
     *   byte 0  = Report ID (0x01-0x04)
     *   byte 1  = �� report �ڵ���Ч STK �ֽ���
     *   byte 2..= STK message bytes
     *
     * hidapi �ᰴ���� Feature Report ���ȷ��ͣ�β������������ֽڡ�?
     * ��������ϸ�? byte1 ȡ��Ч���ݣ����ܰ�����ֽ��ͽ�? STK ״̬����
     */
    if (len >= 2U &&
        data[0] == reportId &&
        reportId >= 1U && reportId <= 4U)
    {
        stkPayloadStart = 2U;
        stkLen = data[1];
        if (stkLen > (uint8_t)(len - stkPayloadStart))
        {
            stkLen = (uint8_t)(len - stkPayloadStart);
        }
    }
    else
    {
        /* ���ݾɵ�������: �ҵ� STK_STX ��ֻ�������ʵ�ʴ��ڵ��ֽڡ�? */
        stkPayloadStart = len;
        stkLen = 0U;
        for (i = 0; i < len; i++)
        {
            if (data[i] == STK_STX)
            {
                stkPayloadStart = i;
                stkLen = (uint8_t)(len - i);
                break;
            }
        }
    }

    for (i = stkPayloadStart; i < (uint8_t)(stkPayloadStart + stkLen) && i < len; i++)
    {
        stkSetRxChar(STK_DATA_SOURCE_USB_HID, data[i]);
    }

    stkPoll();
    HID_ResetRequestState();
}
void HID_Task(void)
{
    stkPoll();
}

uint8_t HID_Rx_IsAvailable(void)
{
    return 0;
}

uint8_t HID_Rx_Read(uint8_t *buf, uint8_t maxLen)
{
    (void)buf;
    (void)maxLen;
    return 0;
}

uint8_t *HID_GetReport_Buffer(uint8_t reportId, uint16_t requestedLen, uint16_t *pOutLen)
{
    static uint8_t reportBuf[HID_REPORT_MAX_LOAD];
    uint16_t reportLen;
    uint16_t payloadSize;
    uint16_t txCount;
    uint16_t idx;
    int c;

    if (pOutLen == NULL) { return NULL; }

    payloadSize = HID_GetPayloadSize(reportId);
    if (payloadSize == 0U) { *pOutLen = 0; return NULL; }

    reportLen = (uint16_t)(payloadSize + 1U);
    if (requestedLen != 0U && requestedLen < reportLen) { reportLen = requestedLen; }
    if (reportLen > HID_REPORT_MAX_LOAD) { reportLen = HID_REPORT_MAX_LOAD; }
    if (reportLen < 2U) { *pOutLen = 0; return NULL; }

    /* Zero-fill the whole output report */
    for (idx = 0; idx < reportLen; idx++) { reportBuf[idx] = 0; }

    reportBuf[0] = reportId;

    txCount = (uint16_t)stkGetTxCount(STK_DATA_SOURCE_USB_HID);
    reportBuf[1] = 0U;

    idx = 2U;
    while (idx < reportLen && (c = stkGetTxByte(STK_DATA_SOURCE_USB_HID)) >= 0)
    {
        reportBuf[idx++] = (uint8_t)c;
    }
    reportBuf[1] = (uint8_t)(idx - 2U);

#if IS_DEBUG_USB
    uart1_WriteString("HID TX len=");
    HidDebugWriteDec(reportLen);
    uart1_WriteString("\r\n");
#endif
    *pOutLen = reportLen;
    return reportBuf;
}

void HID_ResetRequestState(void)
{
    g_RequestType = REQUEST_TYPE_IDLE;
    g_HidReportId = 0;
}


