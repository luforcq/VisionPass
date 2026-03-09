#ifndef MPU6050_H
#define MPU6050_H
/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: ap3216creg.h
作者	  	: 左忠凯
版本	   	: V1.0
描述	   	: MPU6050寄存器地址描述头文件
其他	   	: 无
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2019/9/2 左忠凯创建
***************************************************************/

#define MPU6050_ADDR    	0X68	/* MPU6050器件地址  */

/* AP3316C寄存器 */
#define MPU6050_PWR_MGMT	0x6B	/* 电源管理寄存器       */
#define MPU6050_SMPLRT_DIV	0x19	/* 分频器寄存器       */
#define MPU6050_SYSTEMCONG	0x1A	/* 配置寄存器       */
#define MPU6050_GYRO_CONFIG	0x1B	/* 配置寄存器       */
#define MPU6050_ACCEL_CONFIG	0x1C	/* 配置寄存器       */
#define MPU6050_DATALOW 	0x3B	/* 配置寄存器       */

#define MPU6050_INTSTATUS	0X01	/* 中断状态寄存器   */
#define MPU6050_INTCLEAR	0X02	/* 中断清除寄存器   */
#define MPU6050_IRDATALOW	0x0A	/* IR数据低字节     */
#define MPU6050_IRDATAHIGH	0x0B	/* IR数据高字节     */
#define MPU6050_ALSDATALOW	0x0C	/* ALS数据低字节    */
#define MPU6050_ALSDATAHIGH	0X0D	/* ALS数据高字节    */
#define MPU6050_PSDATALOW	0X0E	/* PS数据低字节     */
#define MPU6050_PSDATAHIGH	0X0F	/* PS数据高字节     */

#endif

