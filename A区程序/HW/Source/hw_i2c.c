#include "hw_i2c.h"
#include "gd32f10x.h"
#include "delay.h"
#include "os_port.h"  


void HW_I2C_Init(void)
{
    rcu_periph_clock_enable(RCU_GPIOB);	
    // 必须是开漏输出 (Open-Drain)，靠外部上拉电阻拉高，这样才能读 SDA！
    gpio_init(GPIOB, GPIO_MODE_OUT_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_6 | GPIO_PIN_7);
    

    IIC_SDA_H;
    for(int i = 0; i < 9; i++) {
        IIC_SCL_H;
        Delay_Us(5);
        IIC_SCL_L;
        Delay_Us(5);
    }
    
    // 恢复正常空闲状态
    IIC_SCL_H;
    IIC_SDA_H;
}

// ==========================================
// 起始信号
// ==========================================
void HW_I2C_Start(void)
{
    OSAL_EnterCritical(); //  开启护盾：绝对不允许 RTOS 打断起始时序！
    IIC_SCL_H;
    IIC_SDA_H;
    Delay_Us(2);
    IIC_SDA_L;
    Delay_Us(2);
    IIC_SCL_L;
    OSAL_ExitCritical();  //  关闭护盾
}

// ==========================================
// 停止信号
// ==========================================
void HW_I2C_Stop(void)
{
    OSAL_EnterCritical();
    IIC_SCL_L;
    IIC_SDA_L;
    Delay_Us(2);
    IIC_SCL_H;
    Delay_Us(2);
    IIC_SDA_H;
    OSAL_ExitCritical();
}

// ==========================================
//  绝技二：原子级字节发送
// ==========================================
void HW_I2C_Send_Byte(uint8_t txd)
{
    int8_t i;
    
    OSAL_EnterCritical(); // 开启护盾：发这 8 个 bit 的几十微秒内，天王老子来了也不能切任务！
    for(i = 7; i >= 0; i--) {
        IIC_SCL_L;
        if(txd & BIT(i))
            IIC_SDA_H;
        else
            IIC_SDA_L;
        Delay_Us(2);
        
        IIC_SCL_H;
        Delay_Us(2);
    }
    IIC_SCL_L;
    IIC_SDA_H; // 释放 SDA 线，准备接下来的 ACK 读取
    OSAL_ExitCritical();  //  字节发完，立刻关闭护盾，把 CPU 还给系统处理其他中断！
}

// ==========================================
// 极其健壮的等待应答
// ==========================================
uint8_t HW_I2C_Wait_Ack(void)
{
    int16_t timeout = 100; // 超时机制极其重要！
    
    OSAL_EnterCritical();
    IIC_SDA_H; // 主机释放SDA
    Delay_Us(1);
    IIC_SCL_H;
    Delay_Us(1);
    
    while(READ_SDA) {
        timeout--;
        if(timeout == 0) {
            // 超时了！从机装死！
            IIC_SCL_L;
            OSAL_ExitCritical();
            return 1; // 返回错误码 1
        }
        Delay_Us(1);
    }
    
    IIC_SCL_L;
    OSAL_ExitCritical();
    return 0; // 成功收到 ACK
}

// ==========================================
// 极其纯粹的读字节
// ==========================================
uint8_t HW_I2C_Read_Byte(uint8_t ack)
{
    int8_t i;
    uint8_t rxd = 0;
    
    OSAL_EnterCritical(); //  开启护盾
    IIC_SDA_H; // 确保主机释放SDA，否则读不到从机的数据
    
    for(i = 7; i >= 0; i--) {
        IIC_SCL_L;
        Delay_Us(2);
        IIC_SCL_H;
        if(READ_SDA)
            rxd |= BIT(i);
        Delay_Us(2);
    }
    IIC_SCL_L;
    OSAL_ExitCritical();  //  关闭护盾
    
    // 处理应答信号
    if(ack) {
        // 发送 ACK (SDA拉低)
        OSAL_EnterCritical();
        IIC_SDA_L;
        Delay_Us(2);
        IIC_SCL_H;
        Delay_Us(2);
        IIC_SCL_L;
        OSAL_ExitCritical();
    } else {
        // 发送 NACK (SDA拉高)
        OSAL_EnterCritical();
        IIC_SDA_H;
        Delay_Us(2);
        IIC_SCL_H;
        Delay_Us(2);
        IIC_SCL_L;
        OSAL_ExitCritical();
    }
    
    return rxd;
}
