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
//#include "my_mpu6050.h"

#define ADS1115_ADDR_POINTER  0x00  // 指针寄存器
#define ADS1115_ADDR_CONFIG   0x01  // 配置寄存器
#define ADS1115_ADDR_CONVERT  0x02  // 转换结果寄存器

// ADS1115 配置参数
#define ADS1115_CFG_OS_SINGLE  0x8000  // 单次转换模式
#define ADS1115_CFG_MUX_AIN0  0x0000  // 通道 AIN0
#define ADS1115_CFG_PGA_4096  0x0200  // 增益 ±4.096V
#define ADS1115_CFG_MODE_SINGLE 0x0100 // 单次模式
#define ADS1115_CFG_DR_128    0x0080  // 128SPS
#define ADS1115_CFG_CQUE_NONE 0x0003  // 禁用比较器

#define ADS1115_CNT	1
#define ADS1115_NAME	"my_ads1115"

struct ads1115_dev {
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备 	 */
	struct device_node	*nd; /* 设备节点 */
	int major;			/* 主设备号 */
	void *private_data;	/* 私有数据 */
	s16  adc01,adc02;		/* 三个光传感器数据 ax,ay,az,gx,gy,gz,*/
};

static struct ads1115_dev ads1115dev;

/*
 * @description	: 从mpu6050读取多个寄存器数据
 * @param - dev:  mpu6050设备
 * @param - reg:  要读取的寄存器首地址
 * @param - val:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int ads1115_read_regs(struct ads1115_dev *dev, u8 reg, void *val, int len)
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
static s32 ads1115_write_regs(struct ads1115_dev *dev, u8 reg, u8 *buf, u8 len)
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
static unsigned char ads1115_read_reg(struct ads1115_dev *dev, u8 reg)
{
	u8 data = 0;

	ads1115_read_regs(dev, reg, &data, 1);
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
static void ads1115_write_reg(struct ads1115_dev *dev, u8 reg, u8 data)
{
	u8 buf = 0;
	buf = data;
	ads1115_write_regs(dev, reg, &buf, 1);
}

/*
 * @description	: 读取mpu6050的数据，读取原始数据，包括ALS,PS和IR, 注意！
 *				: 如果同时打开ALS,IR+PS的话两次数据读取的时间间隔要大于112.5ms
 * @param - ir	: ir数据
 * @param - ps 	: ps数据
 * @param - ps 	: als数据 
 * @return 		: 无。
 */
void ads1115_readdata(struct ads1115_dev *dev)
{u8 buf[2];
    u8 buf1[2];
	u16 val=0xC383;

    buf1[0] = (val >> 8) & 0xFF;  // 高8位
    buf1[1] = val & 0xFF;         // 低8位
ads1115_write_regs(&ads1115dev, 0X01, buf1,2);
	mdelay(10);	
		ads1115_read_regs(dev, 0x00,buf,2);
	dev->adc01=(s16)((buf[0]<<8)|buf[1]);
			val=0xD383;	

   buf1[0] = (val >> 8) & 0xFF;  // 高8位
    buf1[1] = val & 0xFF;         // 低8位
ads1115_write_regs(&ads1115dev, 0X01, buf1,2);
	mdelay(10);		
	ads1115_read_regs(dev, 0x00,buf,2);
	dev->adc02=(s16)((buf[0]<<8)|buf[1]);
    // dev->adc01=ads1115_read_reg(dev, 0x00,2);
	// dev->adc02=ads1115_read_reg(dev, 0x00+1);

}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int ads1115_open(struct inode *inode, struct file *filp)
{ 
	u8 buf[2];
	u16 val=0xC383;
	filp->private_data = &ads1115dev;
// unsigned short config;
// config |=ADS1115_CFG_OS_SINGLE;
// config =0xC383;

	/* 初始化mpu6050 */
	   
    // ADS1115是大端模式，高字节先写
    buf[0] = (val >> 8) & 0xFF;  // 高8位
    buf[1] = val & 0xFF;         // 低8位
ads1115_write_regs(&ads1115dev, 0X01, buf,2);
	mdelay(10);														/* mpu6050复位最少10ms 	*/

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
static ssize_t ads1115_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
	struct ads1115_dev *dev = (struct ads1115_dev *)filp->private_data;
		s16 data[2];
	long err = 0;

	
	
	ads1115_readdata(dev);

	 data[0] = dev->adc01;
	 data[1] = dev->adc02;
	err = copy_to_user(buf, data, sizeof(data));
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int ads1115_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* mpu6050操作函数 */
static const struct file_operations ads1115_ops = {
	.owner = THIS_MODULE,
	.open = ads1115_open,
	.read = ads1115_read,
	.release = ads1115_release,
};

 /*
  * @description     : i2c驱动的probe函数，当驱动与
  *                    设备匹配以后此函数就会执行
  * @param - client  : i2c设备
  * @param - id      : i2c设备ID
  * @return          : 0，成功;其他负值,失败
  */
static int ads1115_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	 printk(KERN_INFO "ads1115 probe function start!\n");
	/* 1、构建设备号 */
	if (ads1115dev.major) {
		ads1115dev.devid = MKDEV(ads1115dev.major, 0);
		register_chrdev_region(ads1115dev.devid, ADS1115_CNT, ADS1115_NAME);
	} else {
		alloc_chrdev_region(&ads1115dev.devid, 0, ADS1115_CNT, ADS1115_NAME);
		ads1115dev.major = MAJOR(ads1115dev.devid);
	}

	/* 2、注册设备 */
	cdev_init(&ads1115dev.cdev, &ads1115_ops);
	cdev_add(&ads1115dev.cdev, ads1115dev.devid, ADS1115_CNT);

	/* 3、创建类 */
	ads1115dev.class = class_create(THIS_MODULE, ADS1115_NAME);
	if (IS_ERR(ads1115dev.class)) {
		return PTR_ERR(ads1115dev.class);
	}

	/* 4、创建设备 */
	ads1115dev.device = device_create(ads1115dev.class, NULL, ads1115dev.devid, NULL, ADS1115_NAME);
	if (IS_ERR(ads1115dev.device)) {
		return PTR_ERR(ads1115dev.device);
	}

	ads1115dev.private_data = client;

	return 0;
}

/*
 * @description     : i2c驱动的remove函数，移除i2c驱动的时候此函数会执行
 * @param - client 	: i2c设备
 * @return          : 0，成功;其他负值,失败
 */
static int ads1115_remove(struct i2c_client *client)
{
	/* 删除设备 */
	cdev_del(&ads1115dev.cdev);
	unregister_chrdev_region(ads1115dev.devid, ADS1115_CNT);

	/* 注销掉类和设备 */
	device_destroy(ads1115dev.class, ads1115dev.devid);
	class_destroy(ads1115dev.class);
	return 0;
}

/* 传统匹配方式ID列表 */
static const struct i2c_device_id ads1115_id[] = {
	{"ti,ads1115", 0},  
	{}
};

/* 设备树匹配列表 */
static const struct of_device_id ads1115_of_match[] = {
	{ .compatible = "ti,ads1115" },
	{ /* Sentinel */ }
};

/* i2c驱动结构体 */	
static struct i2c_driver ads1115_driver = {
	.probe = ads1115_probe,
	.remove = ads1115_remove,
	.driver = {
			.owner = THIS_MODULE,
		   	.name = "ads1115_name",
		   	.of_match_table = ads1115_of_match, 
		   },
	.id_table = ads1115_id,
};
		   
/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init ads1115_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&ads1115_driver);
	return ret;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit ads1115_exit(void)
{
	i2c_del_driver(&ads1115_driver);
}

/* module_i2c_driver(ads1115_driver) */

module_init(ads1115_init);
module_exit(ads1115_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");



