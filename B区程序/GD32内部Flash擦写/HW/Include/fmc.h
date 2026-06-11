#ifndef FMC_H
#define FMC_H

#include "stdint.h"

//  参数全部升级为 uint32_t (真实物理地址 + 真实字节数)
void GD32_EraseFlash(uint32_t start_addr, uint32_t total_bytes);
void GD32_WriteFlash(uint32_t saddr, uint32_t *wdata, uint32_t wnum_bytes);

#endif /* FMC_H */

