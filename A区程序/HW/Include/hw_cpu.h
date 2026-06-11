#ifndef HW_CPU_H
#define HW_CPU_H

#include <stdint.h>

// ==========================================
// CPU 内核级控制接口
// ==========================================

// IAP 终极点火跳转函数
void HW_CPU_JumpToAddress(uint32_t address);

// 强行软重启函数 (OTA 结束后调用这个自杀重启)
void HW_CPU_Reset(void);

#endif /* HW_CPU_H */
