/*
 * ????: stm32f10x_it.c
 * ????: USB ?? / USB ????
 * ????: ???
 * ????: ??????? Bootloader ??????????????
 * ????: ????????????????????????????? GB2312/CP936 ?????
 */
/*
 * STM32F10x中断服务例程 - USB相关中断处理
 */

/******************** (C) COPYRIGHT 2008 STMicroelectronics ********************
* File Name          : stm32f10x_it.c
* Author             : MCD Application Team
* Version            : V2.2.0
* Date               : 06/13/2008
* Description        : Main Interrupt Service Routines.
*                      This file provides template for all exceptions handler
*                      and peripherals interrupt service routine.
********************************************************************************
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "usb_istr.h"
#include "usb_lib.h"
#include "usb_pwr.h"
#include "platform_config.h"
#include "usart.h"

static void bootFaultWriteHex32(uint32_t value)
{
  int shift;
  for (shift = 28; shift >= 0; shift -= 4)
  {
    uint8_t nibble = (uint8_t)((value >> shift) & 0x0FU);
    uart1_WriteBytePolling((u8)(nibble < 10U ? ('0' + nibble) : ('A' + nibble - 10U)));
  }
}

static void bootFaultWriteReg(const char *tag, uint32_t value)
{
  uart1_WriteStringPolling(tag);
  bootFaultWriteHex32(value);
  uart1_WriteStringPolling("\r\n");
}

static void bootFaultHalt(const char *tag)
{
  while (1)
  {
    uart1_WriteStringPolling(tag);
  }
}

void bootHardFaultReport(uint32_t *stackedRegs)
{
  uint32_t r0   = stackedRegs[0];
  uint32_t r1   = stackedRegs[1];
  uint32_t r2   = stackedRegs[2];
  uint32_t r3   = stackedRegs[3];
  uint32_t r12  = stackedRegs[4];
  uint32_t lr   = stackedRegs[5];
  uint32_t pc   = stackedRegs[6];
  uint32_t xpsr = stackedRegs[7];

  uart1_WriteStringPolling("FAULT:HF\r\n");
  bootFaultWriteReg("HF_R0=", r0);
  bootFaultWriteReg("HF_R1=", r1);
  bootFaultWriteReg("HF_R2=", r2);
  bootFaultWriteReg("HF_R3=", r3);
  bootFaultWriteReg("HF_R12=", r12);
  bootFaultWriteReg("HF_LR=", lr);
  bootFaultWriteReg("HF_PC=", pc);
  bootFaultWriteReg("HF_XPSR=", xpsr);
  bootFaultWriteReg("HF_CFSR=", SCB->CFSR);
  bootFaultWriteReg("HF_HFSR=", SCB->HFSR);
  bootFaultWriteReg("HF_DFSR=", SCB->DFSR);
  bootFaultWriteReg("HF_AFSR=", SCB->AFSR);
  bootFaultWriteReg("HF_BFAR=", SCB->BFAR);
  bootFaultWriteReg("HF_MMFAR=", SCB->MMFAR);

  while (1)
  {
  }
}

void NMI_Handler(void)
{
  bootFaultHalt("FAULT:NMI\r\n");
}

__asm void HardFault_Handler(void)
{
  IMPORT bootHardFaultReport
  TST lr, #4
  ITE EQ
  MRSEQ r0, MSP
  MRSNE r0, PSP
  B bootHardFaultReport
}

void MemManage_Handler(void)
{
  bootFaultHalt("FAULT:MEM\r\n");
}

void BusFault_Handler(void)
{
  bootFaultHalt("FAULT:BUS\r\n");
}

void UsageFault_Handler(void)
{
  bootFaultHalt("FAULT:USG\r\n");
}

/*******************************************************************************
* Function Name  : USB_HP_CAN_TX_IRQHandler
* Description    : This function handles USB High Priority or CAN TX interrupts
*                  requests.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void USB_HP_CAN1_TX_IRQHandler(void)
{

}

/*******************************************************************************
* Function Name  : USB_LP_CAN_RX0_IRQHandler
* Description    : This function handles USB Low Priority or CAN RX0 interrupts
*                  requests.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void USB_LP_CAN1_RX0_IRQHandler(void)
{
  USB_Istr();
}
/*******************************************************************************
* Function Name  : USBWakeUp_IRQHandler
* Description    : This function handles USB WakeUp interrupt request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void USBWakeUp_IRQHandler(void)
{
	EXTI->PR|=1<<18;//清除USB唤醒中断挂起位
}
			   
/******************* (C) COPYRIGHT 2008 STMicroelectronics *****END OF FILE****/  


