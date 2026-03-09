#include <stdio.h>	
#include <stdlib.h>      
#include <fcntl.h>      
#include <sys/ioctl.h>   
#include <asm/ioctls.h>
#include <unistd.h> 
#include <stdint.h>
#include "rc522.h"
#include <string.h>


int main(int argc, const char * argv [ ])
{
    int ret = -1;
	uint8_t buf[2];
	ret = PcdReset();
	if(ret != 0)
	{
       	printf("rc522 rst error %d \n",ret);
	   	return 0;
	}
	M500PcdConfigISOType ( 'A' );

	    // ========== 2. SPI回环测试：写TxControlReg→读验证 ==========
    printf("=== SPI通信测试 ===\n");
    // 先读初始值
    uint8_t tx_ctrl_init = ReadRawRC(TxControlReg);
    printf("TxControlReg初始值：%02X\n", tx_ctrl_init);

    // 强制写入0x83，延时1ms再读
    WriteRawRC(TxControlReg, 0x83);
    usleep(1000);
    uint8_t tx_ctrl_after_write = ReadRawRC(TxControlReg);
    printf("写入0x83后读取值：%02X\n", tx_ctrl_after_write);

    // 再写入0x00，验证是否能改
    WriteRawRC(TxControlReg, 0x00);
    usleep(1000);
    uint8_t tx_ctrl_after_clear = ReadRawRC(TxControlReg);
    printf("写入0x00后读取值：%02X\n", tx_ctrl_after_clear);

    // 若写入后值不变，直接判定SPI写失效
    if (tx_ctrl_after_write == tx_ctrl_init && tx_ctrl_after_clear == tx_ctrl_init) {
        printf("❌ SPI写寄存器失效！写入指令未生效\n");
        return;
    } else {
        printf("✅ SPI写寄存器功能正常\n");
        // 恢复天线配置
        WriteRawRC(TxControlReg, 0x83);
    }
printf("天线状态：%02X\n", ReadRawRC(TxControlReg));
SetBitMask(TxControlReg, 0x03); 
printf("天线状态：%02X\n", ReadRawRC(TxControlReg)); //我重新检查了一下天线状态，它现在又变回80了，最开始也是80,后面不知道改了什么地方就变成83可以正常运行，但现在又不行了

	test();
    while(1)
    {
	   
	}
}
//给代码加上注释，解释这段代码  根据这段代码告诉我RC522的工作流程  我没有rc522.h，帮我写一份  该项目中的三个代码文件我该分别如何使用 我指的是