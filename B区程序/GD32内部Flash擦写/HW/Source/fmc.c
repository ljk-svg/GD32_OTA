//#include "gd32f10x.h"
//#include "fmc.h"

////  根据你的芯片型号修改！(大容量比如RCT6/VET6填2048，中容量比如C8T6填1024)
//#define GD32_FLASH_PAGE_SIZE  2048  

//// ==========================================
////  工业级擦除：传绝对地址和总字节数
//// ==========================================
//void GD32_EraseFlash(uint32_t start_addr, uint32_t total_bytes)
//{
//    fmc_unlock();
//    
//    // 极其冷酷地清除所有脏标志，保证手术环境绝对无菌！
//    fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
//    
//    uint32_t erase_addr = start_addr;
//    uint32_t end_addr = start_addr + total_bytes;
//    
//    while (erase_addr < end_addr) {
//        fmc_page_erase(erase_addr);
//        // 擦除一页后也清一下标志位，防爆！
//        fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
//        erase_addr += GD32_FLASH_PAGE_SIZE; 
//    }
//    
//    fmc_lock();
//}

//// ==========================================
////  工业级写入：防溢出死机
//// ==========================================
//void GD32_WriteFlash(uint32_t saddr, uint32_t *wdata, uint32_t wnum_bytes)
//{
//    fmc_unlock();
//    
//    // 清理标志位
//    fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
//    
//    // 把字节数换算成 32位(4字节) 的“字”数量，向上取整！绝对不溢出！
//    uint32_t word_cnt = wnum_bytes / 4;
//    if (wnum_bytes % 4 != 0) word_cnt++; 
//    
//    while (word_cnt > 0) {
//        fmc_word_program(saddr, *wdata);
//        
//        // 写完一个字清一下标志
//        fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
//        
//        saddr += 4;
//        wdata++;
//        word_cnt--;
//    }
//    
//    fmc_lock();
//}
#include "gd32f10x.h"

// 重构：精准擦除
void GD32_EraseFlash(uint32_t start_addr, uint32_t size) {
    fmc_unlock(); // 【关键】解锁
    fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);

    uint32_t end_addr = start_addr + size;
    uint32_t cur_addr = start_addr;

    // GD32F103 每页通常是 1KB 或 2KB
    while (cur_addr < end_addr) {
        fmc_page_erase(cur_addr);
        fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
        cur_addr += 1024; // 假设是 1KB 一页，如果不确定可改为 2048
    }
    
    fmc_lock(); // 【关键】锁上
}

// 重构：精准写入
void GD32_WriteFlash(uint32_t addr, uint32_t* data, uint32_t len) {
    fmc_unlock(); // 【关键】解锁
    fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);

    uint32_t word_len = (len + 3) / 4; // 字节转字，向上取整

    for (uint32_t i = 0; i < word_len; i++) {
        fmc_word_program(addr + (i * 4), data[i]);
        fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
        
        // 校验：如果写进去的不相等，说明出错了（可选）
        if (*(uint32_t*)(addr + (i * 4)) != data[i]) {
            // 这里可以加报错打印
        }
    }

    fmc_lock(); // 【关键】锁上
}