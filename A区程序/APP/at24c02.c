#include "at24c02.h"
#include "hw_i2c.h"    //  调用第二层的抽象接口
#include "os_port.h"   //  调用操作系统的延时接口

uint8_t AT24C02_Init(void) {
    HW_I2C_Init();
    return 0;
}

// ==========================================
//  架构师级写单字节：写完必须让出 CPU！
// ==========================================
uint8_t AT24C02_WriteByte(uint8_t addr, uint8_t wdata) 
{
    HW_I2C_Start();
    HW_I2C_Send_Byte(AT24C02_WADDR);
    if(HW_I2C_Wait_Ack() != 0) return 1;
    
    HW_I2C_Send_Byte(addr);
    if(HW_I2C_Wait_Ack() != 0) return 2;
    
    HW_I2C_Send_Byte(wdata);
    if(HW_I2C_Wait_Ack() != 0) return 3;
    
    HW_I2C_Stop();
    
    //  终极灵魂：EEPROM 物理烧录需要 5ms！
    // 绝对不用死循环死等！直接调用 OSAL 挂起当前任务！
    // 把几十万次指令的算力，极其无私地送给其他任务！
    OSAL_DelayMs(5); 
    
    return 0;
}

// ==========================================
// 极其优雅的连续读数据
// ==========================================
uint8_t AT24C02_ReadData(uint8_t addr, uint8_t *rdata, uint16_t datalen) 
{
    uint16_t i;
    
    HW_I2C_Start();
    HW_I2C_Send_Byte(AT24C02_WADDR);
    if(HW_I2C_Wait_Ack() != 0) return 1;
    
    HW_I2C_Send_Byte(addr);
    if(HW_I2C_Wait_Ack() != 0) return 2;
    
    HW_I2C_Start();
    HW_I2C_Send_Byte(AT24C02_RADDR);
    if(HW_I2C_Wait_Ack() != 0) return 3;
    
    // 循环读数据
    for(i = 0; i < datalen - 1; i++) {
        rdata[i] = HW_I2C_Read_Byte(1); // 1 表示发 ACK
    }
    rdata[datalen - 1] = HW_I2C_Read_Byte(0); // 最后一个字节发 NACK(0)
    
    HW_I2C_Stop();
    return 0;	
}

// ==========================================
//  工业级写连续数据（极简版，通过 WriteByte 拼装）
// (如果追求极速，可以用 PageWrite 结合页对齐算法，但跨页极容易出错)
// ==========================================
uint8_t AT24C02_WriteBuffer(uint8_t addr, uint8_t *wdata, uint16_t datalen)
{
    for(uint16_t i = 0; i < datalen; i++) {
        // 调用我们写好的 WriteByte
        // 它每写完一个字节，就会自动切走 5ms 的任务，绝对安全，绝对不会撑爆页！
        if(AT24C02_WriteByte(addr + i, wdata[i]) != 0) {
            return 1; // 写入失败
        }
    }
    return 0;
}
