#ifndef HW_FLASH_H
#define HW_FLASH_H

#include <stdint.h>

// GD32F103C8T6/CBT6 (中容量设备 MD) 的页大小是 1KB
// 如果你用的是大容量(HD, 比如RCT6/VET6)，页大小是 2KB！(这里按你原来的 1KB 写)
#define GD32_FLASH_PAGE_SIZE    1024  
#define GD32_FLASH_BASE_ADDR    0x08000000

void HW_Flash_Init(void);
void HW_Flash_Erase(uint16_t start_page, uint16_t page_num);
void HW_Flash_Write(uint32_t saddr, uint32_t *wdata, uint32_t wnum_bytes);

#endif /* HW_FLASH_H */
