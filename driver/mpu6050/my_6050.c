#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/i2c.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "my_mpu6050.h"
/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: mpu6050.c
作者	  	: 左忠凯
版本	   	: V1.0
描述	   	: mpu6050驱动程序
其他	   	: 无
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2019/9/2 左忠凯创建
***************************************************************/
#define MPU6050_CNT	1
#define MPU6050_NAME	"my_mpu6050"

struct mpu6050_dev {
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备 	 */
	struct device_node	*nd; /* 设备节点 */
	int major;			/* 主设备号 */
	void *private_data;	/* 私有数据 */
	short  ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw;		/* 三个光传感器数据 ax,ay,az,gx,gy,gz,*/
};

static struct mpu6050_dev mpu6050dev;

/*
 * @description	: 从mpu6050读取多个寄存器数据
 * @param - dev:  mpu6050设备
 * @param - reg:  要读取的寄存器首地址
 * @param - val:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int mpu6050_read_regs(struct mpu6050_dev *dev, u8 reg, void *val, int len)
{
	int ret;
	struct i2c_msg msg[2];
	struct i2c_client *client = (struct i2c_client *)dev->private_data;

	/* msg[0]为发送要读取的首地址 */
	msg[0].addr = client->addr;			/* mpu6050地址 */
	msg[0].flags = 0;					/* 标记为发送数据 */
	msg[0].buf = &reg;					/* 读取的首地址 */
	msg[0].len = 1;						/* reg长度*/

	/* msg[1]读取数据 */
	msg[1].addr = client->addr;			/* mpu6050地址 */
	msg[1].flags = I2C_M_RD;			/* 标记为读取数据*/
	msg[1].buf = val;					/* 读取数据缓冲区 */
	msg[1].len = len;					/* 要读取的数据长度*/

	ret = i2c_transfer(client->adapter, msg, 2);
	if(ret == 2) {
		ret = 0;
	} else {
		printk("i2c rd failed=%d reg=%06x len=%d\n",ret, reg, len);
		ret = -EREMOTEIO;
	}
	return ret;
}

/*
 * @description	: 向mpu6050多个寄存器写入数据
 * @param - dev:  mpu6050设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */
static s32 mpu6050_write_regs(struct mpu6050_dev *dev, u8 reg, u8 *buf, u8 len)
{
	u8 b[256];
	struct i2c_msg msg;
	struct i2c_client *client = (struct i2c_client *)dev->private_data;
	
	b[0] = reg;					/* 寄存器首地址 */
	memcpy(&b[1],buf,len);		/* 将要写入的数据拷贝到数组b里面 */
		
	msg.addr = client->addr;	/* mpu6050地址 */
	msg.flags = 0;				/* 标记为写数据 */

	msg.buf = b;				/* 要写入的数据缓冲区 */
	msg.len = len + 1;			/* 要写入的数据长度 */

	return i2c_transfer(client->adapter, &msg, 1);
}

/*
 * @description	: 读取mpu6050指定寄存器值，读取一个寄存器
 * @param - dev:  mpu6050设备
 * @param - reg:  要读取的寄存器
 * @return 	  :   读取到的寄存器值
 */
static unsigned char mpu6050_read_reg(struct mpu6050_dev *dev, u8 reg)
{
	u8 data = 0;

	mpu6050_read_regs(dev, reg, &data, 1);
	return data;

#if 0
	struct i2c_client *client = (struct i2c_client *)dev->private_data;
	return i2c_smbus_read_byte_data(client, reg);
#endif
}

/*
 * @description	: 向mpu6050指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  mpu6050设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */
static void mpu6050_write_reg(struct mpu6050_dev *dev, u8 reg, u8 data)
{
	u8 buf = 0;
	buf = data;
	mpu6050_write_regs(dev, reg, &buf, 1);
}

/*
 * @description	: 读取mpu6050的数据，读取原始数据，包括ALS,PS和IR, 注意！
 *				: 如果同时打开ALS,IR+PS的话两次数据读取的时间间隔要大于112.5ms
 * @param - ir	: ir数据
 * @param - ps 	: ps数据
 * @param - ps 	: als数据 
 * @return 		: 无。
 */
void mpu6050_readdata(struct mpu6050_dev *dev)
{
	unsigned char i =0;
    unsigned char buf[14];
	
	/* 循环读取所有传感器数据 */
    for(i = 0; i < 14; i++)	
    {
        buf[i] = mpu6050_read_reg(dev, MPU6050_DATALOW + i);	
    }

    dev->ax_raw=(short)((buf[0]<<8)|buf[1]);
    dev->ay_raw=(short)((buf[2]<<8)|buf[3]);
	dev->az_raw=(short)((buf[4]<<8)|buf[5]);

	dev->gx_raw=(short)((buf[8]<<8)|buf[9]);
	dev->gy_raw=(short)((buf[10]<<8)|buf[11]);
	dev->gz_raw=(short)((buf[12]<<8)|buf[13]);

	// dev->ax=dev->ax_raw*1.2207e-4f;
	// dev->ay=dev->ay_raw*1.2207e-4f;
	// dev->az=dev->az_raw*1.2207e-4f;

	// dev->gx=dev->gx_raw*3.05175e-2f;
	// dev->gy=dev->gy_raw*3.05175e-2f;
	// dev->gz=dev->gz_raw*3.05175e-2f;

}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int mpu6050_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &mpu6050dev;

	/* 初始化mpu6050 */
	mpu6050_write_reg(&mpu6050dev, MPU6050_PWR_MGMT, 0x80);		/* 复位mpu6050 			*/
	mdelay(100);														/* mpu6050复位最少10ms 	*/
	mpu6050_write_reg(&mpu6050dev, MPU6050_SMPLRT_DIV, 0X02);		/* 开启ALS、PS+IR 		*/
	mpu6050_write_reg(&mpu6050dev, MPU6050_PWR_MGMT, 0x03);
	mpu6050_write_reg(&mpu6050dev, MPU6050_SYSTEMCONG, 0X03);		/* 开启ALS、PS+IR 		*/
	mpu6050_write_reg(&mpu6050dev, MPU6050_GYRO_CONFIG, 0x10);
	mpu6050_write_reg(&mpu6050dev, MPU6050_ACCEL_CONFIG, 0x08);
	return 0;
}

/*
 * @description		: 从设备读取数据 
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t mpu6050_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
	short data[6];
	long err = 0;

	struct mpu6050_dev *dev = (struct mpu6050_dev *)filp->private_data;
	
	mpu6050_readdata(dev);

	data[0] = dev->ax_raw;
	data[1] = dev->ay_raw;
	data[2] = dev->az_raw;
	data[3] = dev->gx_raw;
	data[4] = dev->gy_raw;
	data[5] = dev->gz_raw;
	err = copy_to_user(buf, data, sizeof(data));
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int mpu6050_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* mpu6050操作函数 */
static const struct file_operations mpu6050_ops = {
	.owner = THIS_MODULE,
	.open = mpu6050_open,
	.read = mpu6050_read,
	.release = mpu6050_release,
};

 /*
  * @description     : i2c驱动的probe函数，当驱动与
  *                    设备匹配以后此函数就会执行
  * @param - client  : i2c设备
  * @param - id      : i2c设备ID
  * @return          : 0，成功;其他负值,失败
  */
static int mpu6050_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	/* 1、构建设备号 */
	if (mpu6050dev.major) {
		mpu6050dev.devid = MKDEV(mpu6050dev.major, 0);
		register_chrdev_region(mpu6050dev.devid, MPU6050_CNT, MPU6050_NAME);
	} else {
		alloc_chrdev_region(&mpu6050dev.devid, 0, MPU6050_CNT, MPU6050_NAME);
		mpu6050dev.major = MAJOR(mpu6050dev.devid);
	}

	/* 2、注册设备 */
	cdev_init(&mpu6050dev.cdev, &mpu6050_ops);
	cdev_add(&mpu6050dev.cdev, mpu6050dev.devid, MPU6050_CNT);

	/* 3、创建类 */
	mpu6050dev.class = class_create(THIS_MODULE, MPU6050_NAME);
	if (IS_ERR(mpu6050dev.class)) {
		return PTR_ERR(mpu6050dev.class);
	}

	/* 4、创建设备 */
	mpu6050dev.device = device_create(mpu6050dev.class, NULL, mpu6050dev.devid, NULL, MPU6050_NAME);
	if (IS_ERR(mpu6050dev.device)) {
		return PTR_ERR(mpu6050dev.device);
	}

	mpu6050dev.private_data = client;

	return 0;
}

/*
 * @description     : i2c驱动的remove函数，移除i2c驱动的时候此函数会执行
 * @param - client 	: i2c设备
 * @return          : 0，成功;其他负值,失败
 */
static int mpu6050_remove(struct i2c_client *client)
{
	/* 删除设备 */
	cdev_del(&mpu6050dev.cdev);
	unregister_chrdev_region(mpu6050dev.devid, MPU6050_CNT);

	/* 注销掉类和设备 */
	device_destroy(mpu6050dev.class, mpu6050dev.devid);
	class_destroy(mpu6050dev.class);
	return 0;
}

/* 传统匹配方式ID列表 */
static const struct i2c_device_id mpu6050_id[] = {
	{"alientek,mpu6050", 0},  
	{}
};

/* 设备树匹配列表 */
static const struct of_device_id mpu6050_of_match[] = {
	{ .compatible = "my_mpu6050" },
	{ /* Sentinel */ }
};

/* i2c驱动结构体 */	
static struct i2c_driver mpu6050_driver = {
	.probe = mpu6050_probe,
	.remove = mpu6050_remove,
	.driver = {
			.owner = THIS_MODULE,
		   	.name = "mpu6050_name",
		   	.of_match_table = mpu6050_of_match, 
		   },
	.id_table = mpu6050_id,
};
		   
/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init mpu6050_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&mpu6050_driver);
	return ret;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit mpu6050_exit(void)
{
	i2c_del_driver(&mpu6050_driver);
}

/* module_i2c_driver(mpu6050_driver) */

module_init(mpu6050_init);
module_exit(mpu6050_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");



