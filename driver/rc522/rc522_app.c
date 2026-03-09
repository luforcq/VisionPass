/**
 * @file rc522_app.c
 * @brief RC522 RFID模块应用层驱动实现（Linux平台，基于字符设备驱动）
 * @details 封装RC522模块的底层寄存器操作、卡片通信、密码验证、读写卡等核心功能，
 *          提供标准化的应用接口，适配ISO14443A协议的Mifare 1K/4K卡片
 */
#include <stdlib.h>      // 标准库（sleep()）
#include <fcntl.h>       // 文件操作（open/close）
#include <sys/ioctl.h>   // IO控制（ioctl()）
#include <asm/ioctls.h>  // IO控制宏定义
#include <unistd.h>      // 系统调用（sleep/write/read/close/usleep）
#include <stdio.h>       // 标准输入输出（printf/sprintf）
#include <string.h>      // 字符串操作（memset/memcpy）
#include "rc522.h"       // RC522寄存器定义、指令宏、状态码等头文件

// 读写标志定义
#define ReadFlag 	1   // 读卡标志
#define WriteFlag 	0   // 写卡标志

int fd = 0;              // RC522字符设备文件句柄（对应/dev/rc522）

/**
  * @brief  读取RC522模块单个寄存器的值
  * @param  Address：寄存器地址（对应RC522的寄存器映射）
  * @retval 寄存器当前值
  * @note   通过Linux字符设备驱动读取，底层为SPI通信
  */
uint8_t ReadRawRC( uint8_t Address )
{
	uint8_t buf[1];	
	buf[0] = Address;	        // 待读取的寄存器地址
    read(fd, buf, 1);	        // 从设备文件读取1字节（寄存器值）
	return buf[0];	            // 返回寄存器值
}

/**
  * @brief  向RC522模块单个寄存器写入值
  * @param  Address：寄存器地址
  * @param  Value：要写入寄存器的值
  * @retval 无
  * @note   通过Linux字符设备驱动写入，底层为SPI通信
  */
void WriteRawRC( uint8_t Address, uint8_t Value )
{  
	uint8_t buf[2];	
	buf[0] = Address;   // 寄存器地址
	buf[1] = Value;     // 待写入的值
	write(fd, buf, 2);  // 向设备文件写入2字节（地址+值）
}

/**
  * @brief  对RC522寄存器的指定位进行置位（按位或操作）
  * @param  ucReg：寄存器地址
  * @param  ucMask：置位掩码（对应位为1则置位）
  * @retval 无
  * @note   先读寄存器当前值，再按掩码置位，最后写回
  */
void SetBitMask ( uint8_t ucReg, uint8_t ucMask )  
{
  uint8_t ucTemp;
  ucTemp = ReadRawRC ( ucReg );                  // 读取寄存器当前值
  WriteRawRC ( ucReg, ucTemp | ucMask );         // 置位指定掩码位后写回
}

/**
  * @brief  对RC522寄存器的指定位进行清位（按位与取反操作）
  * @param  ucReg：寄存器地址
  * @param  ucMask：清位掩码（对应位为1则清位）
  * @retval 无
  * @note   先读寄存器当前值，再按掩码清位，最后写回
  */
void ClearBitMask ( uint8_t ucReg, uint8_t ucMask )  
{
  uint8_t ucTemp;
  ucTemp = ReadRawRC ( ucReg );                  // 读取寄存器当前值
  WriteRawRC ( ucReg, ucTemp & ( ~ ucMask) );    // 清位指定掩码位后写回
}

/**
  * @brief  开启RC522天线（启用射频发射）
  * @param  无
  * @retval 无
  * @note   TxControlReg寄存器控制天线发射，置位0x03开启TX1/TX2引脚
  */
void PcdAntennaOn ( void )
{
  uint8_t uc;
  uc = ReadRawRC ( TxControlReg );               // 读取发射控制寄存器
  if ( ! ( uc & 0x03 ) )                         // 若天线未开启
   SetBitMask(TxControlReg, 0x03);	            // 置位0x03开启天线
}

/**
  * @brief  关闭RC522天线（禁用射频发射）
  * @param  无
  * @retval 无
  * @note   清位TxControlReg寄存器的0x03位，关闭TX1/TX2引脚
  */
void PcdAntennaOff ( void )
{
  ClearBitMask ( TxControlReg, 0x03 );	        // 清位0x03关闭天线
}

/**
  * @brief  复位并初始化RC522模块
  * @param  无
  * @retval 0: 复位成功；负数: 打开设备文件失败
  * @note   1. 打开RC522字符设备文件
  *         2. 发送硬件复位指令
  *         3. 配置定时器、调制方式、通信模式等基础参数
  */
int PcdReset(void)
{
	// 打开RC522字符设备文件（读写模式）
	fd = open("/dev/rc522", O_RDWR);
    if(fd < 0)
    {
        printf("open rc522_drv error %d\n", fd);
		return fd; // 打开失败，返回错误码
	}

	// 发送RC522复位指令（CommandReg写0x0F）
	WriteRawRC ( CommandReg, 0x0f );
	
	// 等待复位完成（CommandReg的0x10位为0表示复位完成）
	while ( ReadRawRC ( CommandReg ) & 0x10 );
 
	// 初始化RC522核心参数（适配Mifare卡通信）
    WriteRawRC ( ModeReg, 0x3D );         // 通信模式：0x3D（ISO14443A）
    WriteRawRC ( TReloadRegL, 30 );      // 定时器低位：30
	WriteRawRC ( TReloadRegH, 0 );	      // 定时器高位：0
    WriteRawRC ( TModeReg, 0x8D );	      // 定时器模式：0x8D（内部定时器）
    WriteRawRC ( TPrescalerReg, 0x3E );	  // 定时器分频：0x3E
	WriteRawRC ( TxAutoReg, 0x40 );	      // 发射自动控制：100%ASK调制
	return 0; // 初始化成功
}

/**
  * @brief  配置RC522为指定ISO协议类型
  * @param  ucType：协议类型（'A'表示ISO14443_A）
  * @retval 无
  * @note   仅适配ISO14443A协议（Mifare卡），配置后开启天线
  */
void M500PcdConfigISOType ( uint8_t ucType )
{
	if ( ucType == 'A')                     // 配置为ISO14443_A协议
  {
	    ClearBitMask ( Status2Reg, 0x08 );	// 清位加密标志
        WriteRawRC ( ModeReg, 0x3D );      // 通信模式
		WriteRawRC ( RxSelReg, 0x86 );     // 接收选择
		WriteRawRC( RFCfgReg, 0x7F );      // 射频配置（增益）
		WriteRawRC( TReloadRegL, 30 );     // 定时器低位
		WriteRawRC ( TReloadRegH, 0 );     // 定时器高位
		WriteRawRC ( TModeReg, 0x8D );     // 定时器模式
		WriteRawRC ( TPrescalerReg, 0x3E );// 定时器分频
		usleep(10000);	                    // 延时10ms等待配置生效
		PcdAntennaOn ();	                // 开启天线
   }	 
}

/**
  * @brief  RC522与ISO14443卡片核心通信函数
  * @param  ucCommand：RC522命令字（如认证、收发数据）
  * @param  pInData：发送到卡片的数据缓冲区
  * @param  ucInLenByte：发送数据的字节长度
  * @param  pOutData：接收卡片返回数据的缓冲区
  * @param  pOutLenBit：返回数据的位长度（输出参数）
  * @retval 状态码：MI_OK（成功）/MI_ERR（失败）/MI_NOTAGERR（无卡）
  * @note   1. 配置中断使能
  *         2. 将发送数据写入FIFO
  *         3. 发送命令并等待中断响应
  *         4. 读取FIFO中的返回数据
  */
char PcdComMF522 ( uint8_t ucCommand,
                   uint8_t * pInData, 
                   uint8_t ucInLenByte, 
                   uint8_t * pOutData,
                   uint32_t * pOutLenBit )		
{
  char cStatus = MI_ERR;          // 默认返回错误
  uint8_t ucIrqEn   = 0x00;       // 中断使能掩码
  uint8_t ucWaitFor = 0x00;       // 等待的中断标志
  uint8_t ucLastBits;             // 最后一个字节的有效位
  uint8_t ucN;                    // 临时变量
  uint32_t ul;                    // 循环计数器

  // 根据命令类型配置中断
  switch ( ucCommand )
  {
     case PCD_AUTHENT:		  // Mifare密码认证命令
        ucIrqEn   = 0x12;	  // 使能错误中断+空闲中断
        ucWaitFor = 0x10;	  // 等待空闲中断
        break;
     
     case PCD_TRANSCEIVE:	  // 数据收发命令
        ucIrqEn   = 0x77;	  // 使能TX/RX/空闲/低报警/错误/定时器中断
        ucWaitFor = 0x30;	  // 等待RX中断+空闲中断
        break;
     
     default:
       break;     
  }

  // 配置中断使能（IRqInv置位，IRQ引脚电平反转）
  WriteRawRC ( ComIEnReg, ucIrqEn | 0x80 );
  ClearBitMask ( ComIrqReg, 0x80 );	  // 清除所有中断标志
  WriteRawRC ( CommandReg, PCD_IDLE );// 发送空闲命令，清空之前的状态
  SetBitMask ( FIFOLevelReg, 0x80 );	// 清空FIFO缓冲区

  // 将发送数据写入FIFO
  for ( ul = 0; ul < ucInLenByte; ul ++ )
    WriteRawRC ( FIFODataReg, pInData [ ul ] );
    
  WriteRawRC ( CommandReg, ucCommand );	// 发送核心命令

  // 若为收发命令，启动数据发送
  if ( ucCommand == PCD_TRANSCEIVE )
    SetBitMask(BitFramingReg, 0x80);  

  // 等待中断响应（最大等待时间25ms左右）
  ul = 1000;                             
  do 														        
  {
       ucN = ReadRawRC ( ComIrqReg );	// 读取中断标志寄存器
       ul --;
  } while ( ( ul != 0 ) && ( ! ( ucN & 0x01 ) ) && ( ! ( ucN & ucWaitFor ) ) );	

  ClearBitMask ( BitFramingReg, 0x80 );	 // 停止数据发送

  // 等待超时判断
  if ( ul != 0 )
  {
    // 检查错误标志（Buffer溢出/冲突/奇偶校验/协议错误）
    if ( ! ( ReadRawRC ( ErrorReg ) & 0x1B ) )	
    {
      cStatus = MI_OK;  // 无错误，通信成功
      
      // 检查是否定时器中断（无卡）
      if ( ucN & ucIrqEn & 0x01 )			
        cStatus = MI_NOTAGERR;   
        
      // 处理收发命令的返回数据
      if ( ucCommand == PCD_TRANSCEIVE )
      {
        ucN = ReadRawRC ( FIFOLevelReg );	// 读取FIFO中的字节数
        ucLastBits = ReadRawRC ( ControlReg ) & 0x07;	// 最后一个字节的有效位
        
        // 计算返回数据的总位数
        if ( ucLastBits )
          * pOutLenBit = ( ucN - 1 ) * 8 + ucLastBits;   	
        else
          * pOutLenBit = ucN * 8;      
        
        // 边界检查（最大接收长度MAXRLEN）
        if ( ucN == 0 )		
          ucN = 1;    
        if ( ucN > MAXRLEN )
          ucN = MAXRLEN;   
        
        // 从FIFO读取返回数据
        for ( ul = 0; ul < ucN; ul ++ )
          pOutData [ ul ] = ReadRawRC ( FIFODataReg );   
      }        
    }   
    else
      cStatus = MI_ERR;  // 有错误，返回失败
  }

  // 停止定时器，恢复空闲状态
  SetBitMask ( ControlReg, 0x80 );           
  WriteRawRC ( CommandReg, PCD_IDLE ); 
   
  return cStatus;
}

/**
  * @brief  寻卡操作（检测感应区内的ISO14443A卡片）
  * @param  ucReq_code：寻卡指令
  *         - 0x52：寻所有符合14443A的卡
  *         - 0x26：寻未休眠的卡
  * @param  pTagType：卡片类型（输出参数）
  *         - 0x0400：Mifare One(S50)
  *         - 0x0200：Mifare One(S70)
  *         - 0x4400：Mifare UltraLight
  * @retval 状态码：MI_OK（成功）/MI_ERR（失败）
  * @note   寻卡成功后，pTagType返回卡片类型
  */
char PcdRequest ( uint8_t ucReq_code, uint8_t * pTagType )
{
  char cStatus;  
  uint8_t ucComMF522Buf [ MAXRLEN ]; 
  uint32_t ulLen;

  ClearBitMask ( Status2Reg, 0x08 );	// 清除加密标志
  WriteRawRC ( BitFramingReg, 0x07 );  // 最后一个字节7位有效

  ucComMF522Buf [ 0 ] = ucReq_code;	// 写入寻卡指令

  // 发送寻卡指令并接收响应
  cStatus = PcdComMF522 ( PCD_TRANSCEIVE,	
                          ucComMF522Buf,
                          1, 
                          ucComMF522Buf,
                          & ulLen );	

  // 寻卡成功（返回16位数据）
  if ( ( cStatus == MI_OK ) && ( ulLen == 0x10 ) )	
  {    
     * pTagType = ucComMF522Buf [ 0 ];
     * ( pTagType + 1 ) = ucComMF522Buf [ 1 ];
  }
  else
   cStatus = MI_ERR;

  return cStatus;	 
}

/**
  * @brief  防冲撞操作（多卡同时存在时，选择一张卡）
  * @param  pSnr：卡片序列号（输出参数，4字节UID）
  * @retval 状态码：MI_OK（成功）/MI_ERR（失败）
  * @note   防冲撞成功后，pSnr返回卡片唯一UID
  */
char PcdAnticoll ( uint8_t * pSnr )
{
  char cStatus;
  uint8_t uc, ucSnr_check = 0;
  uint8_t ucComMF522Buf [ MAXRLEN ]; 
  uint32_t ulLen;
  
  ClearBitMask ( Status2Reg, 0x08 );	// 清除加密标志
  WriteRawRC ( BitFramingReg, 0x00);	// 整数字节有效
  ClearBitMask ( CollReg, 0x80 );	    // 清除冲突标志
  
  ucComMF522Buf [ 0 ] = 0x93;	        // 防冲撞指令1
  ucComMF522Buf [ 1 ] = 0x20;	        // 防冲撞指令2

  // 发送防冲撞指令并接收响应
  cStatus = PcdComMF522 ( PCD_TRANSCEIVE, 
                          ucComMF522Buf,
                          2, 
                          ucComMF522Buf,
                          & ulLen);      

  // 防冲撞成功
  if ( cStatus == MI_OK)		            
  {
    // 读取4字节UID
    for ( uc = 0; uc < 4; uc ++ )
    {
       * ( pSnr + uc )  = ucComMF522Buf [ uc ];
       ucSnr_check ^= ucComMF522Buf [ uc ]; // 异或校验
    }
    
    // 校验UID（第5字节为校验位）
    if ( ucSnr_check != ucComMF522Buf [ uc ] )
      cStatus = MI_ERR;    			 
  }
  
  SetBitMask ( CollReg, 0x80 ); // 置位冲突标志
  return cStatus;		
}

/**
  * @brief  RC522硬件CRC16计算
  * @param  pIndata：待计算CRC的数据缓冲区
  * @param  ucLen：数据字节长度
  * @param  pOutData：CRC结果（输出参数，2字节）
  * @retval 无
  * @note   利用RC522内置CRC模块计算，结果低字节在前
  */
void CalulateCRC ( uint8_t * pIndata, 
                 uint8_t ucLen, 
                 uint8_t * pOutData )
{
  uint8_t uc, ucN;

  ClearBitMask(DivIrqReg, 0x04);	// 清除CRC中断标志
  WriteRawRC(CommandReg, PCD_IDLE);	// 空闲命令
  SetBitMask(FIFOLevelReg, 0x80);	// 清空FIFO

  // 待计算数据写入FIFO
  for ( uc = 0; uc < ucLen; uc ++)
    WriteRawRC ( FIFODataReg, * ( pIndata + uc ) );   

  WriteRawRC ( CommandReg, PCD_CALCCRC ); // 启动CRC计算

  // 等待CRC计算完成
  uc = 0xFF;
  do 
  {
      ucN = ReadRawRC ( DivIrqReg );
      uc --;
  } while ( ( uc != 0 ) && ! ( ucN & 0x04 ) );
  
  // 读取CRC结果（低字节、高字节）
  pOutData [ 0 ] = ReadRawRC ( CRCResultRegL );
  pOutData [ 1 ] = ReadRawRC ( CRCResultRegM );		
}

/**
  * @brief  选定卡片（确认操作某一张卡，锁定UID）
  * @param  pSnr：卡片序列号（4字节UID）
  * @retval 状态码：MI_OK（成功）/MI_ERR（失败）
  * @note   选定后，后续操作仅针对该卡
  */
char PcdSelect ( uint8_t * pSnr )
{
  char ucN;
  uint8_t uc;
  uint8_t ucComMF522Buf [ MAXRLEN ]; 
  uint32_t  ulLen;
  
  // 组装选卡指令包
  ucComMF522Buf [ 0 ] = PICC_ANTICOLL1;  // 选卡指令
  ucComMF522Buf [ 1 ] = 0x70;            // 选卡参数
  ucComMF522Buf [ 6 ] = 0;               // UID校验位

  // 写入4字节UID并计算校验位
  for ( uc = 0; uc < 4; uc ++ )
  {
    ucComMF522Buf [ uc + 2 ] = * ( pSnr + uc );
    ucComMF522Buf [ 6 ] ^= * ( pSnr + uc );
  }
  
  // 计算CRC并添加到指令包
  CalulateCRC ( ucComMF522Buf, 7, & ucComMF522Buf [ 7 ] );

  ClearBitMask ( Status2Reg, 0x08 );	// 清除加密标志

  // 发送选卡指令
  ucN = PcdComMF522 ( PCD_TRANSCEIVE,
                     ucComMF522Buf,
                     9,
                     ucComMF522Buf, 
                     & ulLen );
  
  // 选卡成功（返回24位数据）
  if ( ( ucN == MI_OK ) && ( ulLen == 0x18 ) )
    ucN = MI_OK;  
  else
    ucN = MI_ERR;    
  
  return ucN;		
}

/**
  * @brief  验证卡片密码（Mifare卡扇区密码验证）
  * @param  ucAuth_mode：验证模式
  *         - 0x60：验证A密钥
  *         - 0x61：验证B密钥
  * @param  ucAddr：块地址（如0x10、0x11）
  * @param  pKey：6字节密码（如默认0xFFFFFFFFFFFF）
  * @param  pSnr：4字节卡片UID
  * @retval 状态码：MI_OK（成功）/MI_ERR（失败）
  * @note   验证成功后，才能对该扇区进行读写操作
  */
char PcdAuthState ( uint8_t ucAuth_mode, 
                    uint8_t ucAddr, 
                    uint8_t * pKey,
                    uint8_t * pSnr )
{
  char cStatus;
  uint8_t uc, ucComMF522Buf [ MAXRLEN ];
  uint32_t ulLen;
  
  // 组装密码验证指令包
  ucComMF522Buf [ 0 ] = ucAuth_mode;  // 验证模式
  ucComMF522Buf [ 1 ] = ucAddr;       // 块地址

  // 写入6字节密钥
  for ( uc = 0; uc < 6; uc ++ )
    ucComMF522Buf [ uc + 2 ] = * ( pKey + uc );   

  // 写入4字节UID（仅前4字节有效）
  for ( uc = 0; uc < 4; uc ++ )
    ucComMF522Buf [ uc + 8 ] = * ( pSnr + uc );   

  // 发送密码验证指令
  cStatus = PcdComMF522 ( PCD_AUTHENT,
                          ucComMF522Buf, 
                          12,
                          ucComMF522Buf,
                          & ulLen );

  // 验证成功判断（状态码OK + 加密标志置位）
  if ( ( cStatus != MI_OK ) || ( ! ( ReadRawRC ( Status2Reg ) & 0x08 ) ) )
    cStatus = MI_ERR;   
    
  return cStatus;
}

/**
  * @brief  向Mifare卡单个块写入数据
  * @param  ucAddr：块地址（如0x10、0x11）
  * @param  pData：待写入的数据（16字节）
  * @retval 状态码：MI_OK（成功）/MI_ERR（失败）
  * @note   写入前需先验证该扇区密码
  */
char PcdWrite ( uint8_t ucAddr, uint8_t * pData )
{
  char cStatus;
  uint8_t uc, ucComMF522Buf [ MAXRLEN ];
  uint32_t ulLen;
   
  // 组装写块指令头
  ucComMF522Buf [ 0 ] = PICC_WRITE;  // 写块指令
  ucComMF522Buf [ 1 ] = ucAddr;      // 块地址

  // 计算CRC并添加到指令包
  CalulateCRC ( ucComMF522Buf, 2, & ucComMF522Buf [ 2 ] );

  // 发送写块指令头，等待卡片响应
  cStatus = PcdComMF522 ( PCD_TRANSCEIVE,
                          ucComMF522Buf,
                          4, 
                          ucComMF522Buf,
                          & ulLen );

  // 指令头响应校验
  if ( ( cStatus != MI_OK ) || ( ulLen != 4 ) || 
         ( ( ucComMF522Buf [ 0 ] & 0x0F ) != 0x0A ) )
    cStatus = MI_ERR;   
      
  // 指令头响应成功，发送写入数据
  if ( cStatus == MI_OK )
  {
    // 写入16字节数据到缓冲区
    for ( uc = 0; uc < 16; uc ++ )
      ucComMF522Buf [ uc ] = * ( pData + uc );  
    
    // 计算CRC并添加到数据尾
    CalulateCRC ( ucComMF522Buf, 16, & ucComMF522Buf [ 16 ] );

    // 发送写入数据
    cStatus = PcdComMF522 ( PCD_TRANSCEIVE,
                           ucComMF522Buf, 
                           18, 
                           ucComMF522Buf,
                           & ulLen );
    
    // 数据写入响应校验
    if ( ( cStatus != MI_OK ) || ( ulLen != 4 ) || 
         ( ( ucComMF522Buf [ 0 ] & 0x0F ) != 0x0A ) )
      cStatus = MI_ERR;   
  } 	
  return cStatus;		
}

/**
  * @brief  从Mifare卡单个块读取数据
  * @param  ucAddr：块地址（如0x10、0x11）
  * @param  pData：读取数据缓冲区（输出16字节）
  * @retval 状态码：MI_OK（成功）/MI_ERR（失败）
  * @note   读取前需先验证该扇区密码
  */
char PcdRead ( uint8_t ucAddr, uint8_t * pData )
{
  char cStatus;
  uint8_t uc, ucComMF522Buf [ MAXRLEN ]; 
  uint32_t ulLen;
  
  // 组装读块指令头
  ucComMF522Buf [ 0 ] = PICC_READ;   // 读块指令
  ucComMF522Buf [ 1 ] = ucAddr;      // 块地址

  // 计算CRC并添加到指令包
  CalulateCRC ( ucComMF522Buf, 2, & ucComMF522Buf [ 2 ] );
 
  // 发送读块指令并接收响应
  cStatus = PcdComMF522 ( PCD_TRANSCEIVE,
                          ucComMF522Buf,
                          4, 
                          ucComMF522Buf,
                          & ulLen );

  // 读块成功（返回144位数据=16字节+CRC）
  if ( ( cStatus == MI_OK ) && ( ulLen == 0x90 ) )
  {
    // 读取16字节数据到缓冲区
    for ( uc = 0; uc < 16; uc ++ )
      * ( pData + uc ) = ucComMF522Buf [ uc ];   
  }
  else
    cStatus = MI_ERR;   
   
  return cStatus;		
}

/**
  * @brief  命令卡片进入休眠状态
  * @param  无
  * @retval 状态码：MI_OK（成功）
  * @note   休眠后卡片不再响应寻卡指令，需重新唤醒
  */
char PcdHalt( void )
{
	uint8_t ucComMF522Buf [ MAXRLEN ]; 
	uint32_t  ulLen;
  
  // 组装休眠指令包
  ucComMF522Buf [ 0 ] = PICC_HALT;  // 休眠指令
  ucComMF522Buf [ 1 ] = 0;	        // 指令参数
	
  // 计算CRC并添加到指令包
  CalulateCRC ( ucComMF522Buf, 2, & ucComMF522Buf [ 2 ] );
  
  // 发送休眠指令
  PcdComMF522 ( PCD_TRANSCEIVE,
                ucComMF522Buf,
                4, 
                ucComMF522Buf, 
                & ulLen );

  return MI_OK;	
}

/**
  * @brief  简化的IC卡读写封装函数
  * @param  UID：卡片UID（输出参数）
  * @param  KEY：6字节密码
  * @param  RW：读写标志（1=读，0=写）
  * @param  Dat：读写数据缓冲区（读：输出16字节；写：输入16字节）
  * @retval 无
  * @note   固定操作0x10块，流程：寻卡→防冲撞→选卡→密码验证→读写→休眠
  */
void IC_CMT ( uint8_t * UID,
              uint8_t * KEY,
              uint8_t RW,
              uint8_t * Dat )
{
  uint8_t ucArray_ID [ 4 ] = { 0 }; // 临时存储卡片UID
  
  PcdRequest ( 0x52, ucArray_ID ); // 寻卡（所有14443A卡）
  PcdAnticoll ( ucArray_ID );      // 防冲撞（获取UID）
  PcdSelect ( UID );               // 选卡
  PcdAuthState ( 0x60, 0x10, KEY, UID ); // 验证0x10块A密钥

  if ( RW )                        // 读0x10块
    PcdRead ( 0x10, Dat );
  else                             // 写0x10块
    PcdWrite ( 0x10, Dat );
   	 
  PcdHalt ();	// 卡片休眠
}

/**
  * @brief  通用IC卡读写函数（封装完整流程）
  * @param  flag：读写标志（WriteFlag=写，ReadFlag=读）
  * @param  WriteData：写卡数据（16字节，写模式有效）
  * @param  readValue：读卡数据（16字节，读模式有效）
  * @retval 无
  * @note   1. 固定使用0x11块、默认A密钥（0xFFFFFFFFFFFF）
  *         2. 流程：寻卡→防冲撞→选卡→密码验证→读写→打印结果→休眠
  */
void IC_Read_Or_Write(uint8_t flag, unsigned char *WriteData, uint8_t *readValue)
{
	// 默认A密钥（Mifare卡出厂默认密钥）  
	uint8_t KeyValue[]={0xFF ,0xFF, 0xFF, 0xFF, 0xFF, 0xFF};   
	char cStr [ 30 ];                // 打印缓冲区
	uint8_t ucArray_ID [ 4 ];        // 卡片UID缓冲区
	uint8_t ucStatusReturn;          // 操作状态码
	// 寻卡（寻未休眠的卡），失败则重试一次
	if ( ( ucStatusReturn = PcdRequest ( PICC_REQIDL, ucArray_ID ) ) != MI_OK )  
		ucStatusReturn = PcdRequest ( PICC_REQIDL, ucArray_ID );		                                                
//ucStatusReturn = PcdRequest ( PICC_REQALL, ucArray_ID );
	// 寻卡成功
	if ( ucStatusReturn == MI_OK  )
	{

		// 防冲撞（获取UID）
		if ( PcdAnticoll ( ucArray_ID ) == MI_OK )                                                                   
		{
			PcdSelect(ucArray_ID);			// 选卡		
			// 验证0x11块A密钥
			PcdAuthState( PICC_AUTHENT1A, 0x11, KeyValue, ucArray_ID );
			
			// 写卡操作
			if (flag == WriteFlag)
				PcdWrite(0x11, WriteData);
			
        			// 【修复】读操作仅在flag=ReadFlag时执行，且readValue不为NULL
			if (flag == ReadFlag && readValue != NULL)
			{
				if (PcdRead(0x11, readValue) != MI_OK)
				{
					printf("read err!\r\n");
				}
				else
				{
					sprintf ( cStr, "The Card ID is: %02X%02X%02X%02X",
							  ucArray_ID [0], ucArray_ID [1], 
							  ucArray_ID [2], ucArray_ID [3] );
					printf ( "%s\r\n", cStr ); 			
					printf ("readValue: %s\r\n", readValue);
					PcdHalt();
				}
			}
			else if (flag == WriteFlag)
			{
				// 写卡后直接休眠，避免无效读
				PcdHalt();
				printf("写卡完成！\n");
			}
				
			// // 读卡操作
			// if (PcdRead(0x11, readValue) != MI_OK)
			// {
			// 	printf("read err!\r\n");
			// }
			// else
			// {
			// 	// 打印卡片UID
			// 	sprintf ( cStr, "The Card ID is: %02X%02X%02X%02X",
			// 			  ucArray_ID [0], ucArray_ID [1], 
			// 			  ucArray_ID [2], ucArray_ID [3] );
			// 	printf ( "%s\r\n", cStr ); 			
			// 	// 打印读取的数据
			// 	printf ("readValue: %s\r\n", readValue);
			// 	PcdHalt(); // 卡片休眠
			// }
		}				
	}	
}

/**
  * @brief  RC522读写测试函数（循环读卡）
  * @param  无
  * @retval 无
  * @note   1. 循环读取卡片0x11块数据
  *         2. 每读取100000次清空缓冲区
  *         3. 需先调用PcdReset()和M500PcdConfigISOType()初始化
  */
void test(void)
{
    uint8_t readdata[30] = { 0 }; // 读卡数据缓冲区
    uint32_t times = 0;           // 循环计数

    // 注：实际使用时需先执行以下初始化（当前注释掉）
	// PcdReset ();                // 复位RC522
	// M500PcdConfigISOType ( 'A' ); // 配置ISO14443A协议
    
    // 无限循环读卡  
    while (1)
    {
    //   	    uint8_t write_data[16] = {0};
    // uint8_t read_data[16] = {0};
    // strncpy((char *)write_data, "TestWrite_123456", 16);

    //     IC_Read_Or_Write(WriteFlag, write_data, NULL); // 读0x11块
    //     usleep(10000);	根据这个程序写一个写入的程序 不是叫你重新写一个，而是用上里面的函数
        IC_Read_Or_Write(ReadFlag, NULL, readdata); // 读0x11块
        times++;
        if (times >= 100000)          // 计数达100000次，清空缓冲区
       
          memset (readdata, 0, sizeof(readdata));
    }
}