/*
 * ????: led.h
 * ????: ???? / LED ????
 * ????: ???
 * ????: ??????? Bootloader ??????????????
 * ????: ????????????????????????????? GB2312/CP936 ?????
 */
#ifndef __LED_H
#define __LED_H	 
#include "sys.h"
#include "Hardware_Config.h"

#if !defined(HW_LED_ACTIVE) || !defined(HW_LED_RESET) || !defined(HW_LED_HALT)
    #error "Hardware_Config.h must define HW_LED_ACTIVE, HW_LED_RESET, HW_LED_HALT!"
#endif

//LED驱动代码	   

//-------- LED 源头配置（从这里修改硬件连接）---------
// 格式：HARDWARE_LEDx 端口字母,引脚号
// 过渡宏：LED0/LED1 本身 = 位带操作左值，支持直接赋值
// 示例：LED0 = 0   → PBout(5) = 0   (点亮)
//       LED0 = 1   → PBout(5) = 1   (熄灭)
//       LED0 = !LED0 → PBout(5) = !PBout(5) (反转)
#define LED_ACTIVE  ARM_PORT_OUT(GET_PORT_FROM(HW_LED_ACTIVE))(GET_PIN_FROM(HW_LED_ACTIVE))
#define LED_RESET  ARM_PORT_OUT(GET_PORT_FROM(HW_LED_RESET))(GET_PIN_FROM(HW_LED_RESET))
#define LED_HALT  ARM_PORT_OUT(GET_PORT_FROM(HW_LED_HALT))(GET_PIN_FROM(HW_LED_HALT))

// 辅助宏 GET_PORT_FROM/GET_PIN_FROM/ARM_PORT_OUT 定义在 sys.h 中
#define LED_ACTIVE_LIGHT        LED_ACTIVE = 1
#define LED_ACTIVE_DARK         LED_ACTIVE = 0
#define LED_ACTIVE_CHG          LED_ACTIVE ^= 1
#define LED_RESET_LIGHT         LED_RESET = 1
#define LED_RESET_DARK          LED_RESET = 0
#define LED_RESET_CHG           LED_RESET ^= 1
#define LED_HALT_LIGHT          LED_HALT = 1
#define LED_HALT_DARK           LED_HALT = 0
#define LED_HALT_CHG            LED_HALT ^= 1

void LED_Init(void);//初始化
#endif


