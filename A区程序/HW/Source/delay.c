#include "delay.h"
#include "gd32f10x.h"

//  废弃底层硬件初始化！因为 FreeRTOS 会自动接管 SysTick！
void Delay_Init(void){
    // 这里极其高傲地留空！什么都不做！
}

//  纯 CPU 空转的微秒延时（绝对不碰硬件定时器！）
void Delay_Us(uint16_t us){
    // GD32 跑 108MHz，一个 __NOP() 和 while 循环大约消耗几个时钟周期
    // 粗略计算，循环 15 到 20 次大约是 1 微秒 (波形延时只要 >= 1us 就行，不用极度精准)
    uint32_t delay = us * 18; 
    while(delay--) {
        __asm("NOP"); // 告诉编译器，我就要在这里原地踏步，别给我优化掉！
    }
}

//  兼容遗留代码的毫秒延时
// 警告：在 RTOS 任务里，应该用 OSAL_DelayMs，绝对不要用这个！
// 这里保留只是为了防止有些老代码调用报错。
void Delay_Ms(uint16_t ms){
    while(ms--){
        Delay_Us(1000);
    }
}
