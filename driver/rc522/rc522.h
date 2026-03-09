#ifndef __RC522_APP_H__
#define __RC522_APP_H__

#include <stdint.h>      // uint8_t, uint32_t 等类型定义
#include <stddef.h>      // size_t

/* ==================== 应用层宏定义 ==================== */
#define ReadFlag        1   // 读卡标志
#define WriteFlag       0   // 写卡标志

/* ==================== 全局变量声明 ==================== */
extern int fd;           // RC522字符设备文件句柄（对应/dev/rc522）

/* ==================== 常量定义 ==================== */
#define MAXRLEN         64  // 最大接收缓冲区长度

/* ==================== RC522 寄存器地址定义 ==================== */
// PAGE 0
#define     RFU00                 0x00    
#define     CommandReg            0x01    
#define     ComIEnReg             0x02    
#define     DivlEnReg             0x03    
#define     ComIrqReg             0x04    
#define     DivIrqReg             0x05
#define     ErrorReg              0x06    
#define     Status1Reg            0x07    
#define     Status2Reg            0x08    
#define     FIFODataReg           0x09
#define     FIFOLevelReg          0x0A
#define     WaterLevelReg         0x0B
#define     ControlReg            0x0C
#define     BitFramingReg         0x0D
#define     CollReg               0x0E
#define     RFU0F                 0x0F
// PAGE 1     
#define     RFU10                 0x10
#define     ModeReg               0x11
#define     TxModeReg             0x12
#define     RxModeReg             0x13
#define     TxControlReg          0x14
#define     TxAutoReg             0x15
#define     TxSelReg              0x16
#define     RxSelReg              0x17
#define     RxThresholdReg        0x18
#define     DemodReg              0x19
#define     RFU1A                 0x1A
#define     RFU1B                 0x1B
#define     MifareReg             0x1C
#define     RFU1D                 0x1D
#define     RFU1E                 0x1E
#define     SerialSpeedReg        0x1F
// PAGE 2    
#define     RFU20                 0x20  
#define     CRCResultRegM         0x21
#define     CRCResultRegL         0x22
#define     RFU23                 0x23
#define     ModWidthReg           0x24
#define     RFU25                 0x25
#define     RFCfgReg              0x26
#define     GsNReg                0x27
#define     CWGsCfgReg            0x28
#define     ModGsCfgReg           0x29
#define     TModeReg              0x2A
#define     TPrescalerReg         0x2B
#define     TReloadRegH           0x2C
#define     TReloadRegL           0x2D
#define     TCounterValueRegH     0x2E
#define     TCounterValueRegL     0x2F
// PAGE 3      
#define     RFU30                 0x30
#define     TestSel1Reg           0x31
#define     TestSel2Reg           0x32
#define     TestPinEnReg          0x33
#define     TestPinValueReg       0x34
#define     TestBusReg            0x35
#define     AutoTestReg           0x36
#define     VersionReg            0x37
#define     AnalogTestReg         0x38
#define     TestDAC1Reg           0x39  
#define     TestDAC2Reg           0x3A   
#define     TestADCReg            0x3B   
#define     RFU3C                 0x3C   
#define     RFU3D                 0x3D   
#define     RFU3E                 0x3E   
#define     RFU3F		  		        0x3F



/* ==================== RC522 命令定义 ==================== */
#define PCD_IDLE          0x00    // 取消当前命令
#define PCD_AUTHENT       0x0E    // 认证密钥
#define PCD_RECEIVE       0x08    // 接收数据
#define PCD_TRANSMIT      0x04    // 发送数据
#define PCD_TRANSCEIVE    0x0C    // 发送并接收数据
#define PCD_RESETPHASE    0x0F    // 复位
#define PCD_CALCCRC       0x03    // CRC计算

/* ==================== PICC 命令定义 ==================== */
#define PICC_REQIDL       0x26    // 寻天线区内未进入休眠状态的卡
#define PICC_REQALL       0x52    // 寻天线区内所有卡
#define PICC_ANTICOLL1    0x93    // 防冲撞（选择UID级联1）
#define PICC_AUTHENT1A    0x60    // 验证A密钥
#define PICC_AUTHENT1B    0x61    // 验证B密钥
#define PICC_READ         0x30    // 读块
#define PICC_WRITE        0xA0    // 写块
#define PICC_DECREMENT    0xC0    // 减值
#define PICC_INCREMENT    0xC1    // 增值
#define PICC_RESTORE      0xC2    // 备份
#define PICC_TRANSFER     0xB0    // 转存
#define PICC_HALT         0x50    // 休眠

/* ==================== 状态码定义 ==================== */
#define MI_OK             0       // 操作成功
#define MI_ERR            -1      // 操作失败
#define MI_NOTAGERR       -2      // 无卡错误

/* ==================== 函数声明 ==================== */

/**
 * @brief  读取RC522模块单个寄存器的值
 * @param  Address：寄存器地址
 * @retval 寄存器当前值
 */
uint8_t ReadRawRC(uint8_t Address);

/**
 * @brief  向RC522模块单个寄存器写入值
 * @param  Address：寄存器地址
 * @param  Value：要写入的值
 * @retval 无
 */
void WriteRawRC(uint8_t Address, uint8_t Value);

/**
 * @brief  对RC522寄存器的指定位进行置位
 * @param  ucReg：寄存器地址
 * @param  ucMask：置位掩码
 * @retval 无
 */
void SetBitMask(uint8_t ucReg, uint8_t ucMask);

/**
 * @brief  对RC522寄存器的指定位进行清位
 * @param  ucReg：寄存器地址
 * @param  ucMask：清位掩码
 * @retval 无
 */
void ClearBitMask(uint8_t ucReg, uint8_t ucMask);

/**
 * @brief  开启RC522天线
 * @param  无
 * @retval 无
 */
void PcdAntennaOn(void);

/**
 * @brief  关闭RC522天线
 * @param  无
 * @retval 无
 */
void PcdAntennaOff(void);

/**
 * @brief  复位并初始化RC522模块
 * @param  无
 * @retval 0: 成功；负数: 打开设备失败
 */
int PcdReset(void);

/**
 * @brief  配置RC522为指定ISO协议类型
 * @param  ucType：协议类型（'A'表示ISO14443_A）
 * @retval 无
 */
void M500PcdConfigISOType(uint8_t ucType);

/**
 * @brief  RC522与ISO14443卡片核心通信函数
 * @param  ucCommand：RC522命令字
 * @param  pInData：发送数据缓冲区
 * @param  ucInLenByte：发送数据长度
 * @param  pOutData：接收数据缓冲区
 * @param  pOutLenBit：接收数据位长度（输出）
 * @retval 状态码
 */
char PcdComMF522(uint8_t ucCommand, uint8_t *pInData, uint8_t ucInLenByte, 
                 uint8_t *pOutData, uint32_t *pOutLenBit);

/**
 * @brief  寻卡操作
 * @param  ucReq_code：寻卡指令
 * @param  pTagType：卡片类型（输出）
 * @retval 状态码
 */
char PcdRequest(uint8_t ucReq_code, uint8_t *pTagType);

/**
 * @brief  防冲撞操作
 * @param  pSnr：卡片序列号（输出，4字节UID）
 * @retval 状态码
 */
char PcdAnticoll(uint8_t *pSnr);

/**
 * @brief  RC522硬件CRC16计算
 * @param  pIndata：待计算数据
 * @param  ucLen：数据长度
 * @param  pOutData：CRC结果（2字节）
 * @retval 无
 */
void CalulateCRC(uint8_t *pIndata, uint8_t ucLen, uint8_t *pOutData);

/**
 * @brief  选定卡片
 * @param  pSnr：卡片序列号（4字节UID）
 * @retval 状态码
 */
char PcdSelect(uint8_t *pSnr);

/**
 * @brief  验证卡片密码
 * @param  ucAuth_mode：验证模式（0x60/0x61）
 * @param  ucAddr：块地址
 * @param  pKey：6字节密码
 * @param  pSnr：4字节卡片UID
 * @retval 状态码
 */
char PcdAuthState(uint8_t ucAuth_mode, uint8_t ucAddr, uint8_t *pKey, uint8_t *pSnr);

/**
 * @brief  向Mifare卡单个块写入数据
 * @param  ucAddr：块地址
 * @param  pData：待写入数据（16字节）
 * @retval 状态码
 */
char PcdWrite(uint8_t ucAddr, uint8_t *pData);

/**
 * @brief  从Mifare卡单个块读取数据
 * @param  ucAddr：块地址
 * @param  pData：读取数据缓冲区（16字节）
 * @retval 状态码
 */
char PcdRead(uint8_t ucAddr, uint8_t *pData);

/**
 * @brief  命令卡片进入休眠状态
 * @param  无
 * @retval 状态码
 */
char PcdHalt(void);

/**
 * @brief  简化的IC卡读写封装函数
 * @param  UID：卡片UID（输出）
 * @param  KEY：6字节密码
 * @param  RW：读写标志（1=读，0=写）
 * @param  Dat：读写数据缓冲区
 * @retval 无
 */
void IC_CMT(uint8_t *UID, uint8_t *KEY, uint8_t RW, uint8_t *Dat);

/**
 * @brief  通用IC卡读写函数
 * @param  flag：读写标志（WriteFlag/ReadFlag）
 * @param  WriteData：写卡数据（16字节，写模式有效）
 * @param  readValue：读卡数据（16字节，读模式有效）
 * @retval 无
 */
void IC_Read_Or_Write(uint8_t flag, unsigned char *WriteData, uint8_t *readValue);

/**
 * @brief  RC522读写测试函数（循环读卡）
 * @param  无
 * @retval 无
 */
void test(void);

#endif /* __RC522_APP_H__ */