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
#include "Stk500Protocol.h"
#include "Lcd12864Bmp.h"
#include "Lcd12864.h"

#define HW_USB_DP_PORT  A,12

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

    Stm32_Clock_Init(6);
    delay_init(72);
    uart_init(72, 256000);

    LED_Init();

    printf("SystemClk:%ld\r\n", 72000000L);

    SPI_EEPROM_Init();
    if (!stkShouldEnterBootloader())
    {
        (void)stkTryStartApplication();
    }

    usb_port_set(0);
    delay_ms(300);
    usb_port_set(1);

    USB_Interrupts_Config();
    Set_USBClock();
    USB_Init();

    LCD_GPIO_Init();
    LCD_Init();
    LCD_DisplayString58(1, 12, "DIF Micro");
    LCD_DisplayGraphic(1, 1, 64, 64, bmp_defeng_Logo);
    LCD_DisplayGB2312String(3, 9, "DFM");

    while (1)
    {
        CDC_Task();
        HID_Task();
        stkWinUSBTask();
        stkWinUSBFlush();
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
    }
}
