/*
 * ????: sys.h
 * ????: ?????? / ??????
 * ????: ???
 * ????: ??????? Bootloader ??????????????
 * ????: ????????????????????????????? GB2312/CP936 ?????
 */
#ifndef __SYS_H
#define __SYS_H	  
#include <stm32f10x.h>   
//系统时钟初始化		   

//-------- 以下为宏展开辅助，无需修改 --------
// 从 "B,5" 中提取端口和引脚
// GET_PORT_FROM(HARDWARE_LED0)  → B
// GET_PIN_FROM(HARDWARE_LED0)   → 5
#define GET_PORT_FROM(...)       GET_PORT_FROM_(__VA_ARGS__)
#define GET_PORT_FROM_(x, ...) x
#define GET_PIN_FROM(...)        GET_PIN_FROM_(__VA_ARGS__)
#define GET_PIN_FROM_(x, y, ...) y

// 拼接出 PBout、PEout 等位带操作函数名
#define ARM_PORT_OUT(port)      ARM_PORT_OUT_(port)
#define ARM_PORT_OUT_(port)     P##port##out

// 读取端口输入值：ARM_PORT_IN(B) → PBin
#define ARM_PORT_IN(port)       ARM_PORT_IN_(port)
#define ARM_PORT_IN_(port)      P##port##in

// 端口结构体指针：ARM_PORT_GPIO(B) → GPIOB
#define ARM_PORT_GPIO(port)     ARM_PORT_GPIO_(port)
#define ARM_PORT_GPIO_(port)    GPIO##port

// CRL/CRH 寄存器：ARM_PORT_CRL(B) → GPIOB->CRL, ARM_PORT_CRH(B) → GPIOB->CRH
#define ARM_PORT_CRL(port)      (ARM_PORT_GPIO(port)->CRL)
#define ARM_PORT_CRH(port)      (ARM_PORT_GPIO(port)->CRH)

// RCC 时钟使能：ARM_PORT_RCC_CLK(B) 使能 GPIOB 时钟
#define ARM_PORT_RCC_CLK(port)  (RCC->APB2ENR |= (1 << (ARM_PORT_RCC_BIT(port))))
#define ARM_PORT_RCC_BIT(port)  ARM_PORT_RCC_BIT_(port)
#define ARM_PORT_RCC_BIT_(port) ARM_PORT_RCC_BIT_##port
#define ARM_PORT_RCC_BIT_A      2
#define ARM_PORT_RCC_BIT_B      3
#define ARM_PORT_RCC_BIT_C      4
#define ARM_PORT_RCC_BIT_D      5
#define ARM_PORT_RCC_BIT_E      6
#define ARM_PORT_RCC_BIT_F      7
#define ARM_PORT_RCC_BIT_G      8
// 最终打开端口时钟使用的宏定义
#define PORT_RCC_CLK(io)        ARM_PORT_RCC_CLK(GET_PORT_FROM(io))

// GPIO 方向/模式设置辅助宏：
// cfg4bit 为 STM32F103 CRL/CRH 中对应引脚的 4bit 配置值
// 例如：
//   0x3 = 50MHz 通用推挽输出
//   0x7 = 50MHz 通用开漏输出
//   0xB = 50MHz 复用推挽输出
//   0xF = 50MHz 复用开漏输出
//   0x4 = 浮空输入
//   0x8 = 上拉/下拉输入
//   0x0 = 模拟输入
#define ARM_PORT_SET_CFG_(port, pin, cfg4bit) \
    do { \
        if ((pin) < 8U) \
            ARM_PORT_CRL(port) = (ARM_PORT_CRL(port) & ~((u32)0x0FU << (((pin) & 0x07U) << 2))) | ((u32)(cfg4bit) << (((pin) & 0x07U) << 2)); \
        else \
            ARM_PORT_CRH(port) = (ARM_PORT_CRH(port) & ~((u32)0x0FU << (((pin) & 0x07U) << 2))) | ((u32)(cfg4bit) << (((pin) & 0x07U) << 2)); \
    } while (0)

// 50MHz 通用推挽输出：CNF=00 MODE=11 => 0x3
#define PORT_SET_DIR_PP(io) \
    ARM_PORT_SET_CFG_(GET_PORT_FROM(io), GET_PIN_FROM(io), 0x03U)

// 50MHz 通用开漏输出：CNF=01 MODE=11 => 0x7
#define PORT_SET_DIR_OUT_OC(io) \
    ARM_PORT_SET_CFG_(GET_PORT_FROM(io), GET_PIN_FROM(io), 0x07U)

// 50MHz 复用推挽输出：CNF=10 MODE=11 => 0xB
#define PORT_SET_DIR_OUT_MUX_PP(io) \
    ARM_PORT_SET_CFG_(GET_PORT_FROM(io), GET_PIN_FROM(io), 0x0BU)

// 50MHz 复用开漏输出：CNF=11 MODE=11 => 0xF
#define PORT_SET_DIR_OUT_MUX_OC(io) \
    ARM_PORT_SET_CFG_(GET_PORT_FROM(io), GET_PIN_FROM(io), 0x0FU)

// 浮空输入：CNF=01 MODE=00 => 0x4
#define PORT_SET_DIR_IN_FLOAT(io) \
    ARM_PORT_SET_CFG_(GET_PORT_FROM(io), GET_PIN_FROM(io), 0x04U)

// 模拟输入：CNF=00 MODE=00 => 0x0
#define PORT_SET_DIR_AIN(io) \
    ARM_PORT_SET_CFG_(GET_PORT_FROM(io), GET_PIN_FROM(io), 0x00U)

// 上拉输入：CNF=10 MODE=00 => 0x8，且 ODR 对应位写 1
#define PORT_SET_DIR_IN_PU(io) \
    do { \
        ARM_PORT_SET_CFG_(GET_PORT_FROM(io), GET_PIN_FROM(io), 0x08U); \
        PORT_OUT(io) = 1; \
    } while (0)

// 下拉输入：CNF=10 MODE=00 => 0x8，且 ODR 对应位写 0
#define PORT_SET_DIR_IN_PD(io) \
    do { \
        ARM_PORT_SET_CFG_(GET_PORT_FROM(io), GET_PIN_FROM(io), 0x08U); \
        PORT_OUT(io) = 0; \
    } while (0)

// 兼容旧名字
#define PORT_SET_DIR_IN_UPLOAD(io) PORT_SET_DIR_IN_PU(io)


//-------- 统一的端口/引脚访问宏 --------
#define PORT_OUT(io)            ARM_PORT_OUT(GET_PORT_FROM(io))(GET_PIN_FROM(io))
#define PORT_IN(io)             ARM_PORT_IN(GET_PORT_FROM(io))(GET_PIN_FROM(io))
#define PIN_MASK(io)            (1U << GET_PIN_FROM(io))

#define SYSTEM_SUPPORT_UCOS		0

//位带操作,实现51类似的GPIO控制功能
#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2)) 
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr)) 
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum)) 

//IO口地址映射
#define GPIOA_ODR_Addr    (GPIOA_BASE+12)
#define GPIOB_ODR_Addr    (GPIOB_BASE+12)
#define GPIOC_ODR_Addr    (GPIOC_BASE+12)
#define GPIOD_ODR_Addr    (GPIOD_BASE+12)
#define GPIOE_ODR_Addr    (GPIOE_BASE+12)
#define GPIOF_ODR_Addr    (GPIOF_BASE+12)
#define GPIOG_ODR_Addr    (GPIOG_BASE+12)

#define GPIOA_IDR_Addr    (GPIOA_BASE+8)
#define GPIOB_IDR_Addr    (GPIOB_BASE+8)
#define GPIOC_IDR_Addr    (GPIOC_BASE+8)
#define GPIOD_IDR_Addr    (GPIOD_BASE+8)
#define GPIOE_IDR_Addr    (GPIOE_BASE+8)
#define GPIOF_IDR_Addr    (GPIOF_BASE+8)
#define GPIOG_IDR_Addr    (GPIOG_BASE+8)

//IO口操作,只对单一的IO口!
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr,n)
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)
#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)
#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)
#define PCin(n)    BIT_ADDR(GPIOC_IDR_Addr,n)
#define PDout(n)   BIT_ADDR(GPIOD_ODR_Addr,n)
#define PDin(n)    BIT_ADDR(GPIOD_IDR_Addr,n)
#define PEout(n)   BIT_ADDR(GPIOE_ODR_Addr,n)
#define PEin(n)    BIT_ADDR(GPIOE_IDR_Addr,n)
#define PFout(n)   BIT_ADDR(GPIOF_ODR_Addr,n)
#define PFin(n)    BIT_ADDR(GPIOF_IDR_Addr,n)
#define PGout(n)   BIT_ADDR(GPIOG_ODR_Addr,n)
#define PGin(n)    BIT_ADDR(GPIOG_IDR_Addr,n)

//Ex_NVIC_Config专用定义
#define GPIO_A 0
#define GPIO_B 1
#define GPIO_C 2
#define GPIO_D 3
#define GPIO_E 4
#define GPIO_F 5
#define GPIO_G 6 
#define FTIR   1
#define RTIR   2

//JTAG模式设置定义
#define JTAG_SWD_DISABLE   0X02
#define SWD_ENABLE         0X01
#define JTAG_SWD_ENABLE    0X00	

void Stm32_Clock_Init(u8 PLL);
void Sys_Soft_Reset(void);
void Sys_Standby(void);
void MY_NVIC_SetVectorTable(u32 NVIC_VectTab, u32 Offset);
void MY_NVIC_PriorityGroupConfig(u8 NVIC_Group);
void MY_NVIC_Init(u8 NVIC_PreemptionPriority,u8 NVIC_SubPriority,u8 NVIC_Channel,u8 NVIC_Group);
void Ex_NVIC_Config(u8 GPIOx,u8 BITx,u8 TRIM);
void JTAG_Set(u8 mode);
void WFI_SET(void);
void INTX_DISABLE(void);
void INTX_ENABLE(void);
void MSR_MSP(u32 addr);

#endif
