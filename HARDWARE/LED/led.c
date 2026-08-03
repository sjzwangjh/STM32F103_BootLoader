#include "led.h"
//LED驱动代码	   

//初始化PB5和PE5为输出口.并使能这两个口的时钟		    
//LED IO初始化
void LED_Init(void)
{
	//RCC->APB2ENR|=1<<3;    //使能PORTB时钟	   	 
	//RCC->APB2ENR|=1<<6;    //使能PORTE时钟	
	//   	 
	//GPIOB->CRL&=0XFF0FFFFF; 
	//GPIOB->CRL|=0X00300000;//PB.5 推挽输出   	 
    //GPIOB->ODR|=1<<5;      //PB.5 输出高
	//										  
	//GPIOE->CRL&=0XFF0FFFFF;
	//GPIOE->CRL|=0X00300000;//PE.5推挽输出
	//GPIOE->ODR|=1<<5;      //PE.5输出高 

	// 打开端口时钟
	PORT_RCC_CLK(HW_LED_ACTIVE);
	PORT_RCC_CLK(HW_LED_RESET);
	PORT_RCC_CLK(HW_LED_HALT);
	// 设置端口方向
	PORT_SET_DIR_PP(HW_LED_ACTIVE);
	PORT_SET_DIR_PP(HW_LED_RESET);
	PORT_SET_DIR_PP(HW_LED_HALT);
	// 设置初始状态
	LED_ACTIVE_LIGHT;
	LED_RESET_LIGHT;
	LED_HALT_LIGHT;
}
