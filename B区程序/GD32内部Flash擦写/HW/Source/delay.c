#include "gd32f10x.h"
#include "delay.h"

void Delay_Init(void){
	systick_clksource_set(SYSTICK_CLKSOURCE_HCLK);
}

void Delay_Us(uint16_t us){
	SysTick->LOAD = us*108;
	SysTick->VAL = 0x00;
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
	while(!(SysTick->CTRL&SysTick_CTRL_COUNTFLAG_Msk));
	SysTick->CTRL &=~ SysTick_CTRL_ENABLE_Msk;
}

void Delay_Ms(uint16_t ms){
	while(ms--){
		Delay_Us(1000);
	}
}


