#include "hw_flash.h"
#include "gd32f10x.h"
#include "os_port.h"   

// 内部 Flash 互斥锁（防止两个任务同时往内部 Flash 写数据导致核爆）
static OSAL_Mutex_t internal_flash_mutex = NULL;

void HW_Flash_Init(void) {
    if(internal_flash_mutex == NULL) {
        internal_flash_mutex = OSAL_MutexCreate();
    }
}

// ==========================================
// 内部 Flash 擦除 (按页擦除)
// start_page: 起始页号 (比如 64)
// page_num:   要擦除的页数
// ==========================================
void HW_Flash_Erase(uint16_t start_page, uint16_t page_num) 
{
    uint16_t i;
    
    // 1. 拿锁！天王老子来了也不准在这个时候碰 Flash！
    if(internal_flash_mutex != NULL) {
        OSAL_MutexTake(internal_flash_mutex, OSAL_WAIT_FOREVER);
    }
    
    // 2. 解锁内部 Flash 控制器
    fmc_unlock();
    
    //  3. 极其关键：清除之前的错误标志位！(大厂防死机必备)
    // 防止因为之前某次误操作遗留了错误标志，导致这次擦除直接卡死
    fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);

    // 4. 循环擦除
    for(i = 0; i < page_num; i++) {
        // 计算绝对物理地址
        uint32_t erase_addr = GD32_FLASH_BASE_ADDR + (start_page + i) * GD32_FLASH_PAGE_SIZE;
        
        // 执行擦除 (注意：执行这句时，CPU 会暂停工作，直到擦除完毕)
        fmc_page_erase(erase_addr);
        
        // 擦完一页，清除标志位，准备下一页
        fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
    }
    
    // 5. 上锁 Flash 控制器
    fmc_lock();
    
    // 6. 归还多线程互斥锁！
    if(internal_flash_mutex != NULL) {
        OSAL_MutexGive(internal_flash_mutex);
    }
}

// ==========================================
// 内部 Flash 写入
// saddr:      绝对物理地址 (例如 0x08010000)
// wdata:      数据指针 (注意是 uint32_t*，要求 4 字节对齐)
// wnum_bytes: 要写入的总【字节数】(必须是 4 的整数倍！)
// ==========================================
void HW_Flash_Write(uint32_t saddr, uint32_t *wdata, uint32_t wnum_bytes) 
{
    // 将字节数转换为“字(Word/4字节)”的个数，逻辑极其清晰，绝对不会越界！
    uint32_t word_cnt = wnum_bytes / 4; 
    uint32_t i;

    // 1. 拿锁！
    if(internal_flash_mutex != NULL) {
        OSAL_MutexTake(internal_flash_mutex, OSAL_WAIT_FOREVER);
    }
    
    // 2. 解锁内部 Flash 控制器
    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);

    // 3. 循环写入
    for(i = 0; i < word_cnt; i++) {
        // 编程一个字(32位/4字节)
        fmc_word_program(saddr, wdata[i]);
        
        // 写完清除标志位
        fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
        
        // 地址加 4 (单片机内存地址按字节编址)
        saddr += 4;
    }
    
    // 4. 上锁
    fmc_lock();
    
    // 5. 归还多线程互斥锁！
    if(internal_flash_mutex != NULL) {
        OSAL_MutexGive(internal_flash_mutex);
    }
}
