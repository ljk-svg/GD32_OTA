#include "hw_wdt.h"
#include "gd32f10x.h"

// 硬件看门狗初始化 (约 5 秒超时)
void HW_IWDG_Init(void)
{
    rcu_osci_on(RCU_IRC40K);
    while(SUCCESS != rcu_osci_stab_wait(RCU_IRC40K));

    fwdgt_write_enable();
    fwdgt_config(781, FWDGT_PSC_DIV256); 
    fwdgt_enable();
    fwdgt_counter_reload();
}

// 单纯的底层喂狗动作
void HW_IWDG_Feed(void)
{
    fwdgt_counter_reload();
}
