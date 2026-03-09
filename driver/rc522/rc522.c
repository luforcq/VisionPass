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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/spi/spi.h>
//#include "rc522.h"

#define SPI_NAME    "rc522"
#define SPI_COUNT     1


/* SPI 设备结构体 */
typedef struct rc522_device {
    int major;
    int minor;
    dev_t devid;    /* 设备号 */
    struct cdev cdev;
    struct device *device;
    struct class *class;
    void *private_data;
    struct device_node *node;
    int cs_gpio; /* 片选引脚 */
    int rst_gpio; /* 复位引脚 */
    struct device_node *rst_node;
}Trc522_device, *PTrc522_device;

static Trc522_device TRc522_Device;
// static unsigned char readaddress = 0;

/* 函数声明 */
// void IC_test(void);
static unsigned char Read_One_Reg(PTrc522_device dev, u8 reg);
static void Write_One_Reg(PTrc522_device dev, u8 reg, u8 value);
void RC522_Reset_Disable( void );
void RC522_Reset_Enable(void);
// static void rc522_write_reg(PTrc522_device dev, u8 data, u8 reg);
// static unsigned char rc522_read_reg(PTrc522_device dev, u8 reg);

static int rc522_open(struct inode *inode, struct file *filp)
{
    RC522_Reset_Disable();
    udelay(1);
    RC522_Reset_Enable();
    udelay(1);
    RC522_Reset_Disable();
    udelay(1);

 //   rc522_init();

    printk("rc522_open ok!\n");
    return 0;
}

static ssize_t rc522_read(struct file *file, char __user *buf, size_t cnt, loff_t *ppos)
{
    // u8 
    u8 readval, address;
    int err = 0;
    err = copy_from_user(&address, buf, 1);
    if (err < 0)
    {
      printk("copy from user failed!\r\n");
      return -EFAULT;
    }
    readval = Read_One_Reg(&TRc522_Device, address);
    // // printk("rc522_read ok!\r\n");
    err = copy_to_user((void *)buf, &readval, 1);
    if (err < 0)
    // {
      printk("copy to user failed!\r\n");
    //   return -EFAULT;
    // }
//    printk("rc522_read reg :%#X,  readregval :%#X\r\n", address, readval);
    return 0;
}

static ssize_t rc522_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int ret = 0;
    uint8_t buffer[2];
    ret = copy_from_user(buffer, buf, 2);
    if (ret < 0)
    {
      printk("copy from user failed!\r\n");
    }
    
    Write_One_Reg(&TRc522_Device, buffer[0], buffer[1]);
   
//    printk("write reg: %#X,  writeregval:%#X\n", buffer[0], buffer[1]);
    return 0;
}

static int rc522_release(struct inode *inode, struct file *filp)
{
    int ret = 0;
    RC522_Reset_Enable();
    gpio_free(TRc522_Device.rst_gpio);
    gpio_free(TRc522_Device.cs_gpio);
    printk("rc522_release ok!\n");
    return ret;
}

static struct file_operations TRc522_Device_fops = {
    .owner  = THIS_MODULE,
    .open   = rc522_open,
    .read   = rc522_read,
    .write  = rc522_write,
    .release = rc522_release,
};

/* spi read n reg data from slave dev*/
static int spi_read_regs(PTrc522_device dev, u8 reg, u8 *buf, int len)
{
    int ret = 0;
    u8 txdata[len];
    /* 申请一个spi_message变量 */
    struct spi_message message;
    /* 申请并初始化spi_transfer， spi_transfer此结构体用于描述 SPI 传输信息
    这是内核向驱动提供的API做准备*/
    struct spi_transfer *transfer;
    struct spi_device *spi = (struct spi_device *)dev->private_data;
    /* 拉低GPIO_IO20 表示片选选中GPIO_IO20所连接的spi设备 */
    gpio_set_value(dev->cs_gpio, 0);
    
    transfer = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
    
    // printk("\n读寄存器值函数第一次得到形参reg即要读的寄存器地址 is %#x\n", reg);
    
    /* 第一次，发送要写入的寄存器地址 */
    txdata[0] = ((reg << 1) & 0x7E) | 0x80;    /* csdn :Addr = ( (Address<<1)&0x7E )|0x80;  原子：txdata[0] =reg | 0x80; */
    /* 根据野火stm32例程，此处要加片选使能    
    gpio_set_value(dev->cs_gpio, 0); */                                        
    transfer->tx_buf = txdata;
    transfer->len = 1;

    // printk("读寄存器值函数将reg变为txdata后 is %#x\n", txdata[0]);

    /* 初始化一个spi_message变量 */
    spi_message_init(&message);

    /* 把初始化好的spi_transfer加到message这个队列里来 */
    spi_message_add_tail(transfer, &message);

    /* 使用同步方式传输,同步传输会阻塞的等待 SPI 数据传输完成 */
    ret = spi_sync(spi, &message);

    /* 第二次，读取该寄存器的数据 */
    txdata[0] = 0xff;
    transfer->rx_buf = buf;
    transfer->len = len;

    /* 初始化一个spi_message变量 */
    spi_message_init(&message);

    /* 把初始化好的spi_transfer加到message这个队列里来 */
    spi_message_add_tail(transfer, &message);

    /* 使用同步方式传输,同步传输会阻塞的等待 SPI 数据传输完成 */
    ret = spi_sync(spi, &message);
    kfree(transfer);
    /* 拉高GPIO_IO20，表示通信结束 */
    gpio_set_value(dev->cs_gpio, 1);
    
    // printk("读函数里最终读到的数据是 %#x\n", *buf);

    return ret;
}

// /* spi向从设备的多个寄存器写入数据 */
// static s32 spi_write_regs(PTrc522_device dev, u8 reg, u8 *buf, int len)
// {
//     int ret = 0;
//     u8 txdata[len];
//     /* 申请一个spi_message变量 */
//     struct spi_message message;
//     /* 申请并初始化spi_transfer， spi_transfer此结构体用于描述 SPI 传输信息
//     这是内核向驱动提供的API做准备*/
//     struct spi_transfer *transfer;
//     struct spi_device *spi = (struct spi_device *)dev->private_data;

//     transfer = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
//     /* 拉低GPIO_IO20 表示片选选中GPIO_IO20所连接的spi设备 */
//     gpio_set_value(dev->cs_gpio, 0);
//     // printk("\n写寄存器值函数第一次得到形参reg即要写的寄存器地址 is %#x\n", reg);

//     /* 第一次，发送要写入的寄存器地址 */
//     txdata[0] = (reg << 1) & 0x7E;    /* 原子：txdata[0] = reg & ~0x80; */
//                                     /* csdn：( Address<<1 )&0x7E */

//     // printk("写寄存器值函数将reg变为txdata后 is %#x\n", txdata[0]);
//     /* 根据野火stm32例程，此处要加片选使能    
//     gpio_set_value(dev->cs_gpio, 0); */   
//     transfer->tx_buf = txdata;
//     transfer->len = 1;
//     /* 初始化一个spi_message变量 */
//     spi_message_init(&message);

//     /* 把初始化好的spi_transfer加到message这个队列里来 */
//     spi_message_add_tail(transfer, &message);

//     /* 使用同步方式传输,同步传输会阻塞的等待 SPI 数据传输完成 */
//     ret = spi_sync(spi, &message);

//     /* 第二次，发送要写入的寄存器数据 */
//     transfer->tx_buf = buf;
//     transfer->len = len;

//     // printk("\n写寄存器值函数要发送buf is %#x\n", *buf);

//     /* 初始化一个spi_message变量 */
//     spi_message_init(&message);

//     /* 把初始化好的spi_transfer加到message这个队列里来 */
//     spi_message_add_tail(transfer, &message);

//     /* 使用同步方式传输,同步传输会阻塞的等待 SPI 数据传输完成 */
//     ret = spi_sync(spi, &message);
//     kfree(transfer);
//     /* 拉高GPIO_IO20，表示通信结束 */
//     gpio_set_value(dev->cs_gpio, 1);

//     // printk("\n最后写寄存器值函数要发送buf is %#x\n", *buf);

//     return ret;
// }
/* spi向从设备的多个寄存器写入数据 */
static s32 spi_write_regs(PTrc522_device dev, u8 reg, u8 *buf, int len)
{
    int ret = 0;
    u8 txdata[len + 1];  // 修正：原代码txdata长度错误（len是变量，不能直接作为数组长度）
    struct spi_message message;
    struct spi_transfer *transfer;
    struct spi_device *spi = (struct spi_device *)dev->private_data;

    transfer = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
    if (!transfer) {
        printk(KERN_ERR "spi_write_regs: alloc transfer failed!\n");
        return -ENOMEM;
    }

    /* 拉低片选 */
    gpio_set_value(dev->cs_gpio, 0);
  //  printk(KERN_INFO "spi_write_regs: cs_gpio=%d (low), reg=0x%02X, write_buf=0x%02X, len=%d\n",
  //         dev->cs_gpio, reg, buf[0], len);

    /* 构造寄存器地址（RC522 SPI协议：写地址 = (reg<<1) & 0x7E） */
    txdata[0] = (reg << 1) & 0x7E;
 //   printk(KERN_INFO "spi_write_regs: tx_addr=0x%02X (RC522 write addr)\n", txdata[0]);

    /* 第一步：发送寄存器地址 */
    transfer->tx_buf = txdata;
    transfer->len = 1;
    spi_message_init(&message);
    spi_message_add_tail(transfer, &message);
    ret = spi_sync(spi, &message);
    if (ret < 0) {
        printk(KERN_ERR "spi_write_regs: send addr failed! ret=%d\n", ret);
        goto out;
    }

    /* 第二步：发送寄存器数据 */
    transfer->tx_buf = buf;
    transfer->len = len;
    spi_message_init(&message);
    spi_message_add_tail(transfer, &message);
    ret = spi_sync(spi, &message);
    if (ret < 0) {
        printk(KERN_ERR "spi_write_regs: send data failed! ret=%d\n", ret);
        goto out;
    }

out:
    /* 拉高片选 */
    gpio_set_value(dev->cs_gpio, 1);
    kfree(transfer);
   // printk(KERN_INFO "spi_write_regs: cs_gpio=%d (high), ret=%d\n", dev->cs_gpio, ret);
    return ret;
}

/* 读一个寄存器的值 */
static unsigned char Read_One_Reg(PTrc522_device dev, u8 reg)
{
    // u8 ret = 0;
    // unsigned char tx_buf[1];
    // unsigned char rx_buf[1];
    // tx_buf[0] = reg; //RC522 set ((reg << 1) & 0x7E) | 0x80
    // /* 拉低GPIO_IO20 表示片选选中GPIO_IO20所连接的spi设备 */
    // ret = gpio_direction_output(dev->cs_gpio, 0);
    // if (ret < 0)
    // {
    //     printk("cs_gpio set 0 failed!\r\n");
    // }
    // spi_write_then_read(dev->private_data, tx_buf, 1, rx_buf, 1);
    // /* 拉高GPIO_IO20，表示通信结束 */
    // gpio_direction_output(dev->cs_gpio, 1);

    // return rx_buf[0];

    u8 data = 0;
	spi_read_regs(dev, reg, &data, 1);
	return data;
}

// /* 写一个寄存器的值 */
// static void Write_One_Reg(PTrc522_device dev, u8 reg, u8 value)
// { 
//     u8 buf = value;
// 	spi_write_regs(dev, reg, &buf, 1);
// }
/* 写一个寄存器的值 */
static void Write_One_Reg(PTrc522_device dev, u8 reg, u8 value)
{ 
    u8 buf = value;
   // printk(KERN_INFO "Write_One_Reg: reg=0x%02X, value=0x%02X\n", reg, value);
    spi_write_regs(dev, reg, &buf, 1);
}

void RC522_Reset_Disable( void )
{
  // gpio_set_value(TRc522_Device.rst_gpio, 1);
//  int ret = 0;
  gpio_set_value(TRc522_Device.rst_gpio, 1);
    // if(ret < 0) {
    //   printk(" set rst_gpio disable failed !\r\n");
    // }
//    return ret;
}

void RC522_Reset_Enable( void )
{
  //gpio_set_value(TRc522_Device.rst_gpio, 0);
//    int ret = 0;
    gpio_set_value(TRc522_Device.rst_gpio, 0);
    // if(ret < 0) {
    //   printk("set rst_gpio enable failed!\r\n");
    // }
    // return ret;
}

void rc522_init(void)
{
    RC522_Reset_Disable();
    udelay(10);
    /* disable cs pin */
    gpio_direction_output(TRc522_Device.cs_gpio, 1);
    udelay(10);
}

static int rc522_probe(struct spi_device *spi)
{
    uint8_t val;
    int ret = 0;
    printk("rc522 probe!\n");
    /* 字符设备框架,注册字符设备驱动 */
    /* 1. 申请设备号 */
    TRc522_Device.major = 0;
    if (TRc522_Device.major){
        TRc522_Device.devid = MKDEV(TRc522_Device.major,0);
        register_chrdev_region(TRc522_Device.devid,SPI_COUNT,SPI_NAME);
    }else
    {
        ret = alloc_chrdev_region(&TRc522_Device.devid,0,SPI_COUNT,SPI_NAME);
        TRc522_Device.major = MAJOR(TRc522_Device.devid);
        TRc522_Device.minor = MINOR(TRc522_Device.devid);
    }
    if (ret < 0){
        printk("Failed to alloc devid!\r\n");
        ret = -EINVAL;
    }
    /* 2. cdev 初始化  */
//    TRc522_Device.cdev.owner = THIS_MODULE;
    cdev_init(&TRc522_Device.cdev,&TRc522_Device_fops);
    ret = cdev_add(&TRc522_Device.cdev,TRc522_Device.devid,SPI_COUNT);
    if (ret < 0) {
        printk("Failed to add cdev!\r\n");
    }

    /* 3. 自动创建节点  */
    TRc522_Device.class = class_create(THIS_MODULE,SPI_NAME);
    if (IS_ERR(TRc522_Device.class))
    {
        printk("class create errno\r\n");
        return PTR_ERR(TRc522_Device.class);
    }
    /* 4. 创建设备 */
    TRc522_Device.device = device_create(TRc522_Device.class,NULL,TRc522_Device.devid,NULL,SPI_NAME);
    if (IS_ERR(TRc522_Device.device))
    {
        printk("device create errno\r\n");
        return PTR_ERR(TRc522_Device.device);
    }

    /* 设置RC522所使用的RST GPIO */
	  /* 1、获取设备节点：rst_node */
    TRc522_Device.rst_node = of_find_node_by_path("/spi_rst");
    if(TRc522_Device.rst_node == NULL) {
      printk("TRc522_Device.rst_node not find!\r\n");
      return -EINVAL;
    } else {
      printk("rst_node find!\r\n");
    }

    /* 2、 获取设备树中的gpio属性，得到RC522 RST 所使用的编号 */
    TRc522_Device.rst_gpio = of_get_named_gpio(TRc522_Device.rst_node, "rst-gpio", 0);
    if(TRc522_Device.rst_gpio < 0) {
      printk("can't get rst_gpio");
      return -EINVAL;
    }
    printk("rst_gpio num = %d\r\n", TRc522_Device.rst_gpio);

    /* 3、设置GPIO1_IO01为输出，并且输出高电平 */
    ret = gpio_direction_output(TRc522_Device.rst_gpio, 1);
    if(ret < 0) {
      printk("can't set gpio!\r\n");
    }

    /* 通过spi获取&ecspi3的节点,也就是rc522的父节点 */
    TRc522_Device.node = of_find_node_by_path("/soc/aips-bus@02000000/spba-bus@02000000/ecspi@02010000");
    if (NULL == TRc522_Device.node)
    {
        printk("device node get failed\r\n");
        return -EINVAL;
    }
    TRc522_Device.cs_gpio = of_get_named_gpio(TRc522_Device.node, "cs-gpio", 0);
    if (TRc522_Device.cs_gpio < 0)
    {
        printk("cs_gpio get failed!\r\n");
        return -EINVAL;
    }

    gpio_direction_output(TRc522_Device.cs_gpio, 1);
    spi->mode = SPI_MODE_0;
    spi_setup(spi);
    /* 设置TRc522_Device的成员变量 private_data 为 spi_device */
    TRc522_Device.private_data = spi;

    // 初始化rc522 
    rc522_init();   // 这句可要可不要
//    test();

// // 1. 读取TxControlReg当前值
// val = rc522_read_reg(spi, TxControlReg); // 替换为你驱动中的读寄存器函数
// printk(KERN_INFO "rc522: TxControlReg init val: 0x%02X\n", val);
// // 2. 置位0x03，强制开启天线
// val |= 0x03;
// rc522_write_reg(spi, TxControlReg, val); // 替换为你驱动中的写寄存器函数
// // 3. 再次读取验证
// val = rc522_read_reg(spi, TxControlReg);
// printk(KERN_INFO "rc522: TxControlReg after set: 0x%02X\n", val);
    return 0;
}

static int rc522_remove(struct spi_device *spi)
{
    int ret = 0;
    // gpio_direction_output(TRc522_Device.cs_gpio, 1);
    // if (ret < 0)
    // {
    //     printk("cs_gpio set 0 failed!\r\n");
    // }
    printk("rc522_remove!\r\n");
    gpio_free(TRc522_Device.cs_gpio);
    gpio_free(TRc522_Device.rst_gpio);
    cdev_del(&TRc522_Device.cdev);
    unregister_chrdev_region(TRc522_Device.devid,SPI_COUNT);
    device_destroy(TRc522_Device.class,TRc522_Device.devid);
    class_destroy(TRc522_Device.class);
    return ret;
}

static const struct spi_device_id rc522_id[] = {
    {"alientek,rc522", 0},
    {}
};

static const struct of_device_id rc522_of_match[] = {
    {.compatible = "alientek,rc522"},
    {/* Sentinel */}
};

/* spi driver 结构体 */
static struct spi_driver rc522_driver = {
    .driver = {
        .name   = "Rc522_SPI",
        .owner  = THIS_MODULE,
        .of_match_table = rc522_of_match,
    },
    .probe     = rc522_probe,
    .remove    = rc522_remove,
    .id_table  = rc522_id,
};

/* 驱动入口函数 */
static int __init Rc522_Spi_Init(void) 
{
    int ret = 0;
    spi_register_driver(&rc522_driver);
    return ret;
}

/* 驱动出口函数 */
static void __exit Rc522_Spi_Exit(void)
{
    spi_unregister_driver(&rc522_driver);
}

/* 加载驱动 */
module_init(Rc522_Spi_Init);
/* 卸载驱动 */
module_exit(Rc522_Spi_Exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("OSS");
