#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "rc522.h"
// 必须包含的常量定义（与原文件一致）
// #define MI_OK          0
// #define MI_ERR         -1
// #define MI_NOTAGERR    -2
// #define MAXRLEN        18

// // RC522寄存器/命令/卡片指令（与原文件一致）
// #define CommandReg     0x01
// #define ComIEnReg      0x02
// #define ComIrqReg      0x04
// #define FIFOLevelReg   0x05
// #define FIFODataReg    0x09
// #define ErrorReg       0x06
// #define Status2Reg     0x08
// #define BitFramingReg  0x0D
// #define ControlReg     0x0C
// #define DivIrqReg      0x05
// #define CRCResultRegL  0x0B
// #define CRCResultRegM  0x0C
// #define ModeReg        0x11
// #define TReloadRegL    0x12
// #define TReloadRegH    0x13
// #define TModeReg       0x14
// #define TPrescalerReg  0x15
// #define TxAutoReg      0x16
// #define RxSelReg       0x17
// #define RFCfgReg       0x18
// #define CollReg        0x0E
// #define TxControlReg   0x19  // 原文件PcdAntennaOn用到的寄存器

// #define PCD_IDLE       0x00
// #define PCD_AUTHENT    0x0E
// #define PCD_TRANSCEIVE 0x0C
// #define PCD_CALCCRC    0x03
// #define PCD_RESET      0x0F

// #define PICC_REQIDL    0x26
// #define PICC_REQALL    0x52
// #define PICC_ANTICOLL1 0x93
// #define PICC_AUTHENT1A 0x60
// #define PICC_WRITE     0xA0
// #define PICC_READ      0x30
// #define PICC_HALT      0x50

// #define WriteFlag      1
// #define ReadFlag       0

// 全局变量（与原文件一致）
int fd;


/**
 * @brief  基于原文件函数的写卡主函数（直接复用IC_Read_Or_Write）
 * @note   1. 完全复用原文件的IC_Read_Or_Write、PcdReset等函数
 *         2. 固定写入0x11块，使用默认A密钥0xFFFFFFFFFFFF
 *         3. 流程：初始化→写卡→验证写入结果
 */
int main(int argc, char *argv[])
{
 	if (argc != 2) {
		printf("Error Usage!\r\n");
		return -1;
	}
        uint8_t ucArray_ID[4] = {0};  // 卡片UID缓冲区
    uint8_t write_data[16] = {0}; // 待写入的数据（16字节）
    uint8_t KeyValue[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // 默认密钥
    int write_success = 0;        // 写卡成功标志

    // 1. 初始化RC522（必须先执行）
    printf("RC522 init...\n");
    if (PcdReset() != 0) {
        printf("RC522 init failed! Exit.\n");
        return;
    }
    M500PcdConfigISOType('A');
    printf("RC522 init success. Waiting for card...\n");

    // 2. 填充待写入的数据（按需修改）
   // strncpy((char *)write_data, "TestWrite_456", 16);
 strncpy((char *)write_data, argv[1], 16);
    // 3. 无限循环检测卡片，直到写卡成功
    while (!write_success)
    {
        // 3.1 寻卡（检测是否有卡片靠近）
        if (PcdRequest(PICC_REQIDL, ucArray_ID) == MI_OK)
        {
            printf("Card detected! Start write...\n");

            // 3.2 防冲撞（获取卡片UID）
            if (PcdAnticoll(ucArray_ID) == MI_OK)
            {
                // 3.3 选卡（锁定当前卡片）
                if (PcdSelect(ucArray_ID) == MI_OK)
                {
                    // 3.4 验证扇区密码（0x11块A密钥）
                    if (PcdAuthState(PICC_AUTHENT1A, 0x11, KeyValue, ucArray_ID) == MI_OK)
                    {
                        // 3.5 写入数据到0x11块
                        if (PcdWrite(0x11, write_data) == MI_OK)
                        {
                            printf("Write data success! Data: %s\n", write_data);
                            write_success = 1; // 写卡成功，退出循环
                        }
                        else
                        {
                            printf("Write data failed! Retry...\n");
                        }
                    }
                    else
                    {
                        printf("Auth key failed! Retry...\n");
                    }
                }
                else
                {
                    printf("Select card failed! Retry...\n");
                }
            }
            else
            {
                printf("Anticoll failed! Retry...\n");
            }

            // 3.6 卡片休眠（无论成败，休眠后重新检测）
            PcdHalt();
        }

        // 3.7 无卡时短暂休眠（降低CPU占用，100ms检测一次）
        usleep(100000);
    }

    // 4. 写卡成功，退出程序
    printf("Program exit successfully.\n");
    close(fd); // 关闭RC522设备文件
    exit(0);
    return 0;
}
