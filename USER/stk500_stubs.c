/**
 * @file    stk500_stubs.c
 * @brief   STM32F103VE Bootloader — Flash Update State Machine
 *
 * Three-channel frame buffer (HID / CDC / WinUSB) → STK500v2 parser
 * → flash programming of application area.
 *
 * App area: 0x0800C000 - 0x0807FFFF (464 KB, pages 24-255)
 */
#include "delay.h"
#include "sys.h"
#include "Stk500Protocol.h"
#include "usb_winusb_user.h"
#include "hw_config.h"
#include "eeprom.h"
#include "usart.h"
#include <string.h>

/* Defined in main.c: controls USB D+/transceiver. */
extern void usb_port_set(u8 enable);

/* ── Bootloader constants ──────────────────────────────── */
#define APP_START_ADDER     0x0800C000U
#define FLASH_PAGE_SIZE     2048U
#define FLASH_APP_END       0x08080000U
#define STK_FRAME_MAX       281U          /* header + body + checksum */
#define NUM_SOURCES         3U            /* HID=0, CDC=1, WinUSB=2 */

#define BOOT_LOAD_VERSION   "DFM 01.00.00-0000"
#define EEPROM_BOOT_MODE_ADDR       0x0200U
#define EEPROM_APP_VALID_ADDR       0x0201U
#define EEPROM_BOOT_MODE_UPDATE     0xFFU
#define EEPROM_BOOT_MODE_APP        0x00U
#define EEPROM_APP_VALID_VALUE      0xA5U
#define APP_VECTOR_STACK_MASK       0x2FFE0000U
#define APP_VECTOR_STACK_VALUE      0x20000000U

/* STK500v2 commands handled */
#define CMD_SIGN_ON         0x01U
#define CMD_LOAD_ADDRESS    0x06U
#define CMD_ENTER_PROGMODE  0x10U
#define CMD_LEAVE_PROGMODE  0x11U
#define CMD_CHIP_ERASE      0x12U
#define CMD_PROGRAM_FLASH   0x13U
#define CMD_READ_FLASH      0x14U
#define CMD_READ_SIGNATURE  0x1BU
#define CMD_READ_VERSION    0x1CU
#define CMD_START_APP       0x1DU

/* STK500v2 response codes */
#define STATUS_CMD_OK       0x00U
#define STATUS_CMD_FAILED   0x01U
#define STATUS_CMD_UNKNOWN  0x03U

/* ── Per-source frame assembly buffer ──────────────────── */
typedef struct {
    uint8_t  buf[STK_FRAME_MAX];
    uint16_t pos;
    uint16_t bodyLen;
    uint8_t  seqNum;
    uint8_t  active;
} stkChannel_t;

static stkChannel_t g_chan[NUM_SOURCES];

/* ── TX response buffer (one response at a time) ───────── */
static uint8_t  g_txBuf[STK_FRAME_MAX];
static uint16_t g_txLen;
static uint8_t  g_txSource;

/* ── Flash programming state ───────────────────────────── */
static uint32_t g_flashAddr;
static uint8_t  g_inProgMode;
static uint8_t  g_pageBuf[FLASH_PAGE_SIZE];
static uint32_t g_pageBaseAddr = 0xFFFFFFFFU;
static uint8_t  g_pageDirty;
static uint8_t  g_startAppPending;
static uint32_t g_programByteCount;

/* ── Forward declarations ──────────────────────────────── */
static void stkResetChannel(uint8_t src);
static void stkProcessByte(uint8_t src, uint8_t c);
static void stkProcessFrame(uint8_t src);
static void stkBuildResponse(uint8_t src, uint8_t status, const uint8_t *data, uint16_t len);
static void pageBufferReset(void);
static void pageBufferInit(uint32_t pageBase);
static uint8_t pageBufferFlush(void);
static uint8_t pageBufferWrite(uint32_t addr, const uint8_t *data, uint16_t len);
static uint8_t appVectorLooksValid(void);
static uint8_t appReadyToStart(void);
static void markUpdateMode(void);
static void markApplicationValid(void);
static void jumpToApplication(void);

static void   flashUnlock(void);
static void   flashLock(void);
static uint8_t flashWaitReady(void);
static uint8_t flashErasePage(uint32_t pageAddr);
static uint8_t flashEraseAppArea(void);
static uint8_t flashProgramHalfWords(uint32_t addr, const uint8_t *data, uint16_t len);
static void   flashReadBytes(uint32_t addr, uint8_t *buf, uint16_t len);
static uint8_t flashIsAppArea(uint32_t addr, uint16_t len);

#if IS_DEBUG_USB
static void stkDebugWriteHex32(uint32_t value);
static void stkDebugPrintAddress(const char *tag, uint32_t addr);
#endif

/* ══════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════ */

void stkSetRxChar(uint8_t source, uint8_t data)
{
    if (source < NUM_SOURCES) stkProcessByte(source, data);
}

void stkFeedBytes(uint8_t source, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    if (data == NULL) return;
    for (i = 0; i < len; i++)
        stkSetRxChar(source, data[i]);
}

uint16_t stkPoll(void)
{
    return g_txLen;
}

uint16_t stkGetTxCount(uint8_t source)
{
    return (g_txLen > 0 && g_txSource == source) ? g_txLen : 0;
}

int stkGetTxByte(uint8_t source)
{
    static uint16_t txPos;
    if (g_txLen == 0 || g_txSource != source) { txPos = 0; return -1; }
    if (txPos >= g_txLen) { txPos = 0; g_txLen = 0; return -1; }
    return g_txBuf[txPos++];
}

uint32_t stkGetFlashAddress(void)
{
    return g_flashAddr;
}

uint8_t stkShouldEnterBootloader(void)
{
    if (!appReadyToStart()) {
        return 1U;
    }
    return (SPI_EEPROM_ReadByte(EEPROM_BOOT_MODE_ADDR) == EEPROM_BOOT_MODE_APP) ? 0U : 1U;
}

uint8_t stkTryStartApplication(void)
{
    if (!appReadyToStart()) {
        return 0U;
    }
    jumpToApplication();
    return 1U;
}

void stkService(void)
{
    if (g_startAppPending == 0U) {
        return;
    }
    if (g_txLen != 0U) {
        return;
    }
    delay_ms(200);
    if (!appReadyToStart()) {
        /* Application not valid: keep the bootloader online with USB attached
         * instead of disconnecting and jumping nowhere. */
        g_startAppPending = 0U;
        return;
    }
    /* Replicate the manual RESET-pin path: on this hardware the reliable
     * restart is the NRST button; SYSRESETREQ is not dependable here. So we
     * disconnect USB (D+ low + transceiver power-down), let the supply settle,
     * then jump to the application with the same clean USB state the manual
     * restart produces. */
    usb_port_set(0);
    delay_ms(200);
    jumpToApplication();
    g_startAppPending = 0U;
}

/* ══════════════════════════════════════════════════════════
 *  Channel polling (called from main loop)
 * ══════════════════════════════════════════════════════════ */

void stkWinUSBTask(void)
{
    uint8_t buf[256];
    uint16_t n, i;

    n = WinUSB_Bulk_Available();
    if (n == 0) return;
    if (n > sizeof(buf)) n = sizeof(buf);
    n = WinUSB_Bulk_Recv(buf, n);
    for (i = 0; i < n; i++)
        stkSetRxChar(STK_DATA_SOURCE_USB_WINUSB, buf[i]);
}

void stkWinUSBFlush(void)
{
    uint16_t n;

    if (g_txLen == 0U || g_txSource != STK_DATA_SOURCE_USB_WINUSB) return;
    if (WinUSB_Bulk_TxBusy()) return;

    n = g_txLen;
    if (n > 64U) {
        n = 64U;
    }

    if (WinUSB_Bulk_Send(g_txBuf, n) == 0U) {
        if (n < g_txLen) {
            memmove(g_txBuf, g_txBuf + n, g_txLen - n);
            g_txLen -= n;
        } else {
            g_txLen = 0U;
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  Frame parser
 * ══════════════════════════════════════════════════════════ */

static void stkResetChannel(uint8_t src)
{
    g_chan[src].pos     = 0;
    g_chan[src].bodyLen = 0;
    g_chan[src].active  = 0;
}

static void stkProcessByte(uint8_t src, uint8_t c)
{
    stkChannel_t *ch = &g_chan[src];

    if (!ch->active) {
        if (c == STK_STX) { ch->buf[0] = c; ch->pos = 1; ch->active = 1; }
        return;
    }
    if (ch->pos >= STK_FRAME_MAX) { stkResetChannel(src); return; }

    ch->buf[ch->pos++] = c;

    if (ch->pos == 4) {
        ch->bodyLen = ((uint16_t)ch->buf[2] << 8) | ch->buf[3];
        ch->bodyLen += 6U;
        if (ch->bodyLen > STK_FRAME_MAX) { stkResetChannel(src); return; }
    }
    if (ch->pos == 5 && c != STK_TOKEN) {
        stkResetChannel(src); return;
    }
    if (ch->bodyLen > 0 && ch->pos >= ch->bodyLen) {
        stkProcessFrame(src);
        stkResetChannel(src);
    }
}

/* ══════════════════════════════════════════════════════════
 *  Command dispatcher
 * ══════════════════════════════════════════════════════════ */

static void stkProcessFrame(uint8_t src)
{
    stkChannel_t *ch = &g_chan[src];
    uint16_t flen = ch->pos, bodyLen, i;
    uint8_t  cmd, *payload, xorVal;

    if (flen < 6) return;

    xorVal = 0;
    for (i = 0; i < flen; i++) xorVal ^= ch->buf[i];
    if (xorVal != 0) { stkBuildResponse(src, STATUS_CMD_FAILED, NULL, 0); return; }

    ch->seqNum = ch->buf[1];
    bodyLen = ((uint16_t)ch->buf[2] << 8) | ch->buf[3];
    cmd     = ch->buf[5];
    payload = &ch->buf[6];
    if (bodyLen > 0) bodyLen--;
#if IS_DEBUG_USB
    stkDebugPrintAddress("FRAME", cmd);
#endif

    switch (cmd) {

    case CMD_SIGN_ON: {
        uint8_t r[] = {'S','T','K','5','0','0','_','2'};
        stkBuildResponse(src, STATUS_CMD_OK, r, 8);
        break;
    }
    case CMD_ENTER_PROGMODE:
        g_inProgMode = 1;
        g_startAppPending = 0U;
        g_programByteCount = 0U;
        pageBufferReset();
        markUpdateMode();
        SPI_EEPROM_WriteByte(EEPROM_APP_VALID_ADDR, 0xFFU);
        stkBuildResponse(src, STATUS_CMD_OK, NULL, 0);
        break;
    case CMD_LEAVE_PROGMODE:
#if IS_DEBUG_USB
        uart1_WriteString("LEAVE:case\r\n");
#endif
        if (!g_inProgMode) {
            stkBuildResponse(src, STATUS_CMD_FAILED, NULL, 0);
            break;
        }
#if IS_DEBUG_USB
        uart1_WriteString("LEAVE:flush\r\n");
#endif
        if (!pageBufferFlush()) {
            stkBuildResponse(src, STATUS_CMD_FAILED, NULL, 0);
            break;
        }
        g_inProgMode = 0;
#if IS_DEBUG_USB
        uart1_WriteString("LEAVE:eeprom\r\n");
#endif
        if (g_programByteCount > 0U && appVectorLooksValid()) {
            markApplicationValid();
        }
#if IS_DEBUG_USB
        uart1_WriteString("LEAVE:resp\r\n");
#endif
        stkBuildResponse(src, STATUS_CMD_OK, NULL, 0);
        break;
    case CMD_LOAD_ADDRESS:
        if (bodyLen >= 4) {
            g_flashAddr = ((uint32_t)payload[0] << 24) |
                          ((uint32_t)payload[1] << 16) |
                          ((uint32_t)payload[2] << 8)  |
                           (uint32_t)payload[3];
#if IS_DEBUG_USB
            stkDebugPrintAddress("LOAD", g_flashAddr);
#endif
            stkBuildResponse(src, STATUS_CMD_OK, NULL, 0);
        } else {
            stkBuildResponse(src, STATUS_CMD_FAILED, NULL, 0);
        }
        break;
    case CMD_PROGRAM_FLASH:
        if (!g_inProgMode || bodyLen < 2) {
            stkBuildResponse(src, STATUS_CMD_FAILED, NULL, 0);
            break;
        }
        {
            uint16_t progLen = ((uint16_t)payload[0] << 8) | payload[1];
            if (progLen > (bodyLen - 2U)) progLen = bodyLen - 2U;
            if (!flashIsAppArea(g_flashAddr, progLen)) {
                stkBuildResponse(src, STATUS_CMD_FAILED, NULL, 0);
                break;
            }
            if (pageBufferWrite(g_flashAddr, &payload[2], progLen)) {
                g_flashAddr += progLen;
                g_programByteCount += progLen;
                stkBuildResponse(src, STATUS_CMD_OK, NULL, 0);
            } else {
                stkBuildResponse(src, STATUS_CMD_FAILED, NULL, 0);
            }
        }
        break;
    case CMD_READ_FLASH: {
        uint16_t readLen;
        uint8_t  rbuf[256];
        if (bodyLen < 2) { stkBuildResponse(src, STATUS_CMD_FAILED, NULL, 0); break; }
        readLen = ((uint16_t)payload[0] << 8) | payload[1];
        if (readLen > sizeof(rbuf)) readLen = sizeof(rbuf);
        if (!flashIsAppArea(g_flashAddr, readLen)) {
            stkBuildResponse(src, STATUS_CMD_FAILED, NULL, 0);
            break;
        }
        if (!pageBufferFlush()) {
            stkBuildResponse(src, STATUS_CMD_FAILED, NULL, 0);
            break;
        }
        flashReadBytes(g_flashAddr, rbuf, readLen);
        g_flashAddr += readLen;
        stkBuildResponse(src, STATUS_CMD_OK, rbuf, readLen);
        break;
    }
    case CMD_CHIP_ERASE:
        if (!g_inProgMode) {
            stkBuildResponse(src, STATUS_CMD_FAILED, NULL, 0);
        } else if (flashEraseAppArea()) {
            g_flashAddr = APP_START_ADDER;
            g_programByteCount = 0U;
            pageBufferReset();
            markUpdateMode();
            SPI_EEPROM_WriteByte(EEPROM_APP_VALID_ADDR, 0xFFU);
            stkBuildResponse(src, STATUS_CMD_OK, NULL, 0);
        } else {
            stkBuildResponse(src, STATUS_CMD_FAILED, NULL, 0);
        }
        break;
    case CMD_READ_SIGNATURE: {
        uint8_t sig[] = {0x14, 0x04, 0x00};
        stkBuildResponse(src, STATUS_CMD_OK, sig, 3);
        break;
    }
    case CMD_READ_VERSION: {
        const uint8_t *version = (const uint8_t *)BOOT_LOAD_VERSION;
        stkBuildResponse(src, STATUS_CMD_OK, version, (uint16_t)strlen(BOOT_LOAD_VERSION));
        break;
    }
    case CMD_START_APP:
        if (!appReadyToStart()) {
            stkBuildResponse(src, STATUS_CMD_FAILED, NULL, 0);
            break;
        }
        g_startAppPending = 1U;
        stkBuildResponse(src, STATUS_CMD_OK, NULL, 0);
        break;
    default:
        stkBuildResponse(src, STATUS_CMD_UNKNOWN, NULL, 0);
        break;
    }
}

/* ══════════════════════════════════════════════════════════
 *  Response builder
 * ══════════════════════════════════════════════════════════ */

static void stkBuildResponse(uint8_t src, uint8_t status, const uint8_t *data, uint16_t len)
{
    uint8_t  resp[STK_FRAME_MAX];
    uint16_t rpos = 0, bodyLen, i;

    bodyLen = 1U + len;
    resp[rpos++] = STK_STX;
    resp[rpos++] = g_chan[src].seqNum;
    resp[rpos++] = (uint8_t)(bodyLen >> 8);
    resp[rpos++] = (uint8_t)(bodyLen & 0xFF);
    resp[rpos++] = STK_TOKEN;
    resp[rpos++] = status;
    for (i = 0; i < len; i++) resp[rpos++] = data[i];
    {
        uint8_t xv = 0;
        for (i = 0; i < rpos; i++) xv ^= resp[i];
        resp[rpos++] = xv;
    }
    if (rpos <= sizeof(g_txBuf)) {
        memcpy(g_txBuf, resp, rpos);
        g_txLen    = rpos;
        g_txSource = src;
    }
}


/* ══════════════════════════════════════════════════════════
 *  Flash operations (CMSIS registers)
 * ══════════════════════════════════════════════════════════ */

static void flashUnlock(void)
{
    FLASH->KEYR = 0x45670123U;
    FLASH->KEYR = 0xCDEF89ABU;
}

static void flashLock(void)
{
    FLASH->CR |= 0x00000080U;
}

static uint8_t flashWaitReady(void)
{
    uint32_t sr;
    uint32_t timeout = 0x0FFFFFFFU;
    do {
        sr = FLASH->SR;
        if (timeout-- == 0U) { FLASH->SR = 0x14U; return 0U; }
    } while (sr & 1U);
    if (sr & 0x14U) { FLASH->SR = 0x14U; return 0U; }
    return 1U;
}

static void pageBufferReset(void)
{
    memset(g_pageBuf, 0xFF, sizeof(g_pageBuf));
    g_pageBaseAddr = 0xFFFFFFFFU;
    g_pageDirty = 0U;
}

static void pageBufferInit(uint32_t pageBase)
{
    g_pageBaseAddr = pageBase;
    memset(g_pageBuf, 0xFF, sizeof(g_pageBuf));
}

static uint8_t pageBufferFlush(void)
{
    if (!g_pageDirty) {
        return 1U;
    }
    if (!flashIsAppArea(g_pageBaseAddr, FLASH_PAGE_SIZE)) {
        return 0U;
    }
    if (!flashErasePage(g_pageBaseAddr)) {
        return 0U;
    }
    if (!flashProgramHalfWords(g_pageBaseAddr, g_pageBuf, FLASH_PAGE_SIZE)) {
        return 0U;
    }
#if IS_DEBUG_USB
    stkDebugPrintAddress("PAGE", g_pageBaseAddr);
#endif
    g_pageDirty = 0U;
    return 1U;
}

#if IS_DEBUG_USB
static void stkDebugWriteHex32(uint32_t value)
{
    uint8_t i;
    for (i = 0U; i < 8U; i++) {
        uint8_t nibble = (uint8_t)((value >> (28U - (i * 4U))) & 0x0FU);
        uart1_WriteByte((uint8_t)(nibble < 10U ? ('0' + nibble) : ('A' + nibble - 10U)));
    }
}

static void stkDebugPrintAddress(const char *tag, uint32_t addr)
{
    while (*tag != '\0') {
        uart1_WriteByte((uint8_t)(*tag++));
    }
    uart1_WriteString(" addr=0x");
    stkDebugWriteHex32(addr);
    uart1_WriteString("\r\n");
}
#endif

static uint8_t pageBufferWrite(uint32_t addr, const uint8_t *data, uint16_t len)
{
    while (len > 0U) {
        uint32_t pageBase = addr & ~(FLASH_PAGE_SIZE - 1U);
        uint16_t pageOffset = (uint16_t)(addr - pageBase);
        uint16_t copyLen = (uint16_t)(FLASH_PAGE_SIZE - pageOffset);

        if (copyLen > len) {
            copyLen = len;
        }
        if (!flashIsAppArea(addr, copyLen)) {
            return 0U;
        }
        if (g_pageBaseAddr != pageBase) {
            if (!pageBufferFlush()) {
                return 0U;
            }
            pageBufferInit(pageBase);
        }
        memcpy(&g_pageBuf[pageOffset], data, copyLen);
        g_pageDirty = 1U;
        addr += copyLen;
        data += copyLen;
        len = (uint16_t)(len - copyLen);
    }
    return 1U;
}

static uint8_t appVectorLooksValid(void)
{
    uint32_t appSp = *(volatile uint32_t *)APP_START_ADDER;
    uint32_t appPc = *(volatile uint32_t *)(APP_START_ADDER + 4U);
    uint32_t appEntry = appPc & 0xFFFFFFFEU;

    if ((appSp & APP_VECTOR_STACK_MASK) != APP_VECTOR_STACK_VALUE) {
        return 0U;
    }
    if (appEntry < APP_START_ADDER || appEntry >= FLASH_APP_END) {
        return 0U;
    }
    return 1U;
}

static uint8_t appReadyToStart(void)
{
    if (SPI_EEPROM_ReadByte(EEPROM_APP_VALID_ADDR) != EEPROM_APP_VALID_VALUE) {
        return 0U;
    }
    return appVectorLooksValid();
}

static void markUpdateMode(void)
{
    SPI_EEPROM_WriteByte(EEPROM_BOOT_MODE_ADDR, EEPROM_BOOT_MODE_UPDATE);
}

static void markApplicationValid(void)
{
    SPI_EEPROM_WriteByte(EEPROM_APP_VALID_ADDR, EEPROM_APP_VALID_VALUE);
    SPI_EEPROM_WriteByte(EEPROM_BOOT_MODE_ADDR, EEPROM_BOOT_MODE_APP);
}

static void jumpToApplication(void)
{
    uint32_t appStack = *(volatile uint32_t *)APP_START_ADDER;
    uint32_t appEntry = *(volatile uint32_t *)(APP_START_ADDER + 4U);
    void (*appResetHandler)(void);

    if (!appReadyToStart()) {
        return;
    }

    INTX_DISABLE();
    delay_ms(10);
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;
    MY_NVIC_SetVectorTable(0x08000000U, APP_START_ADDER - 0x08000000U);
    appResetHandler = (void (*)(void))appEntry;
    MSR_MSP(appStack);
    appResetHandler();
}

static uint8_t flashIsAppArea(uint32_t addr, uint16_t len)
{
    uint32_t end = addr + len;
    return (addr >= APP_START_ADDER && end <= FLASH_APP_END);
}

static uint8_t flashErasePage(uint32_t pageAddr)
{
    if (!flashIsAppArea(pageAddr, FLASH_PAGE_SIZE)) {
        return 0U;
    }
    flashUnlock();
    FLASH->CR |= 0x02U;     /* PER */
    FLASH->AR  = pageAddr;
    FLASH->CR |= 0x40U;     /* STRT */
    if (!flashWaitReady()) {
        FLASH->CR &= ~0x02U;
        flashLock();
        return 0U;
    }
    FLASH->CR &= ~0x02U;
    flashLock();
    return 1U;
}

static uint8_t flashEraseAppArea(void)
{
    uint32_t page;
    flashUnlock();
    for (page = APP_START_ADDER; page < FLASH_APP_END; page += FLASH_PAGE_SIZE) {
        FLASH->CR |= 0x02U;     /* PER */
        FLASH->AR  = page;
        FLASH->CR |= 0x40U;     /* STRT */
        if (!flashWaitReady()) { flashLock(); return 0; }
        FLASH->CR &= ~0x02U;
    }
    flashLock();
    return 1;
}

static uint8_t flashProgramHalfWords(uint32_t addr, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    if ((addr & 1) || (len & 1)) return 0;
    flashUnlock();
    for (i = 0; i < len; i += 2) {
        uint16_t hw = ((uint16_t)data[i + 1] << 8) | data[i];
        FLASH->CR |= 0x01U;     /* PG */
        *(volatile uint16_t *)(addr + i) = hw;
        if (!flashWaitReady()) { flashLock(); return 0; }
        FLASH->CR &= ~0x01U;
    }
    flashLock();
    return 1;
}

static void flashReadBytes(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    const uint8_t *src = (const uint8_t *)addr;
    for (i = 0; i < len; i++) buf[i] = src[i];
}
