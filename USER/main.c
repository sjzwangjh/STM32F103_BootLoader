/*
 * ????: main.c
 * ????: ????? / USER
 * ????: ???
 * ????: ??????? Bootloader ??????????????
 * ????: ????????????????????????????? GB2312 ???
 */
#include "sys.h"
#include "usart.h"
#include "delay.h"
#include "led.h"
#include "beep.h"
#include "key.h"
#include "exti.h"
#include "wdg.h"
#include "timer.h"
#include "rtc.h"
#include "wkup.h"
#include "dma.h"
#include "flash.h"
#include "eeprom.h"

#include "usb_lib.h"
#include "hw_config.h"
#include "usb_pwr.h"
#include "usb_cdc_user.h"
#include "usb_hid_user.h"
#include "usb_winusb_user.h"
#include "Stk500Protocol.h"
#include "Lcd12864Bmp.h"
#include "Lcd12864.h"

#define HW_USB_DP_PORT  A,12
#define APP_TRACE_MARK_ADDR 0x2000FFF0U

extern volatile u32 g_wcidReqCnt[8];
extern volatile u32 g_descReqCnt[4];
extern volatile u32 g_eeReqCnt;

void usb_port_set(u8 enable)
{
    RCC->APB2ENR |= 1 << 2;
    if (enable) {
        _SetCNTR(_GetCNTR() & (~(1 << 1)));
    } else {
        _SetCNTR(_GetCNTR() | (1 << 1));
        PORT_SET_DIR_PP(HW_USB_DP_PORT);
        PORT_OUT(HW_USB_DP_PORT) = 0;
    }
}

int main(void)
{
    u8 i = 0;
    u32 resetFlags;
    u32 appTraceMark;

    Stm32_Clock_Init(6);
    delay_init(72);
    uart_init(72, 256000);

    LED_Init();

    resetFlags = RCC->CSR;
    appTraceMark = *(volatile u32 *)APP_TRACE_MARK_ADDR;
    printf("BOOTDBG-4 SystemClk:%ld\r\n", 72000000L);
    printf("BOOT TRACE: mark=0x%08lX\r\n", (unsigned long)appTraceMark);
    printf("BOOT RESET: CSR=0x%08lX\r\n", (unsigned long)resetFlags);
    printf("BOOT RESET: PIN=%u POR=%u SFT=%u IWDG=%u WWDG=%u LPWR=%u\r\n",
           (unsigned int)((resetFlags & RCC_CSR_PINRSTF) ? 1U : 0U),
           (unsigned int)((resetFlags & RCC_CSR_PORRSTF) ? 1U : 0U),
           (unsigned int)((resetFlags & RCC_CSR_SFTRSTF) ? 1U : 0U),
           (unsigned int)((resetFlags & RCC_CSR_IWDGRSTF) ? 1U : 0U),
           (unsigned int)((resetFlags & RCC_CSR_WWDGRSTF) ? 1U : 0U),
           (unsigned int)((resetFlags & RCC_CSR_LPWRRSTF) ? 1U : 0U));
    RCC->CSR |= RCC_CSR_RMVF;

    printf("Boot STEP: eeprom\r\n");
    SPI_EEPROM_Init();
    printf("Boot STEP: bootchk\r\n");
    if (!stkShouldEnterBootloader())
    {
        printf("Boot STEP: apptry\r\n");
        (void)stkTryStartApplication();
        printf("Boot STEP: appret\r\n");
    }

    printf("Boot STEP: usbinit\r\n");
    usb_port_set(0);
    delay_ms(300);

    USB_Interrupts_Config();
    Set_USBClock();
    USB_Init();

    delay_ms(50);
    usb_port_set(1);
    printf("Boot STEP: usbready\r\n");

    LCD_GPIO_Init();
    LCD_Init();
    LCD_DisplayString58(1, 12, "DIF Micro");
    LCD_DisplayGraphic(1, 1, 64, 64, bmp_defeng_Logo);
    LCD_DisplayGB2312String(3, 9, "Boot-");
		LCD_DisplayGB2312String(4, 9, "Loading");

    while (1)
    {
        CDC_Task();      /* USB CDC: RX drain + TX flush (EP3)         */
        HID_Task();      /* USB HID: RX drain + TX flush (EP1)         */
        WinUSB_Task();   /* USB WinUSB Bulk: RX drain + TX flush (EP4) */
        stkService();

        if (bDeviceState == CONFIGURED)
            LED_ACTIVE = 0;
        else
            LED_ACTIVE = 1;

        i++;
        if (i == 200) {
            i = 0;
            LED_ACTIVE = !LED_ACTIVE;
        }

        if (g_descReqCnt[0] || g_descReqCnt[1] || g_descReqCnt[2] || g_descReqCnt[3]
            || g_eeReqCnt || g_wcidReqCnt[4] || g_wcidReqCnt[5] || g_wcidReqCnt[7]) {
            printf("REQ: dev=%lu cfg=%lu str=%lu BOS=%lu EE=%lu w4=%lu w5=%lu w7=%lu\r\n",
                   (unsigned long)g_descReqCnt[0],
                   (unsigned long)g_descReqCnt[1],
                   (unsigned long)g_descReqCnt[2],
                   (unsigned long)g_descReqCnt[3],
                   (unsigned long)g_eeReqCnt,
                   (unsigned long)g_wcidReqCnt[4],
                   (unsigned long)g_wcidReqCnt[5],
                   (unsigned long)g_wcidReqCnt[7]);
            g_descReqCnt[0] = 0;
            g_descReqCnt[1] = 0;
            g_descReqCnt[2] = 0;
            g_descReqCnt[3] = 0;
            g_eeReqCnt = 0;
            g_wcidReqCnt[4] = 0;
            g_wcidReqCnt[5] = 0;
            g_wcidReqCnt[7] = 0;
        }
    }
}
