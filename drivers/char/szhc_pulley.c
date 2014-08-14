/*************************************
NAME:szhc_single_pulley.c
COPYRIGHT:SZHC
*************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/sizes.h>
#include <linux/timer.h>


/* 加载模式后,执行”cat /proc/devices”命令看到的设备名称 */
#define DEVICE_NAME "szhc_pulley"
#define PULLEY_MAJOR 235 /* 主设备号 */



#define MODE2 2
#define MODE7 2


#define IEN (1 << 5)
#define PD (0 << 3)
#define PU (2 << 3)
#define OFF (1 << 3)
#define GPIO_MOD_CTRL_BIT	BIT(0)


#define AM335X_CTRL_BASE			0x44E10000
#define CONTROL_PADCONF_GPMC_ADVN_ALE             0x0890
#define CONTROL_PADCONF_GPMC_OEN_REN              0x0894
#define CONTROL_PADCONF_GPMC_WEN                  0x0898
#define CONTROL_PADCONF_GPMC_CLK                  0x088C

#define MUX_VAL(OFFSET,VALUE)\
    writel((VALUE) | readl(p+(OFFSET)), p + (OFFSET));


#define MUX_EVM() \
    MUX_VAL(CONTROL_PADCONF_GPMC_CLK, (IEN | PU | MODE7 )) /* gpio2[1] */\
    MUX_VAL(CONTROL_PADCONF_GPMC_OEN_REN, (IEN | PU | MODE2 )) /* timer7_mux3 */\
    MUX_VAL(CONTROL_PADCONF_GPMC_WEN, (IEN | PU | MODE2 )) /* timer6_mux3 */

#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

static struct timer_list s_timer;

void initPinOutput(unsigned int gpio,bool isOut,char *label)
{
    int result;
    /* Allocating GPIOs and setting direction */
    result = gpio_request(gpio, label);//usr1
    if (result != 0)
        printk("gpio_request(%d) failed!\n",gpio);
    if(isOut){
        result = gpio_direction_output(gpio,1);
        if (result != 0)
            printk("gpio_direction(%d) failed!\n",gpio);

    }
    else{
        result = gpio_direction_input(gpio);
        if (result != 0)
            printk("gpio_direction(%d) failed!\n",gpio);
    }

}


static void initConfig(void)
{
    void __iomem *p = ioremap_nocache(AM335X_CTRL_BASE ,SZ_4K);
    if(!p){
        printk("ioremap failed\n");
        return;
    }
    MUX_EVM();  /*配置控制模块*/
    iounmap(p);

    /*配置I/O输入*/
    initPinOutput(GPIO_TO_PIN(2,1),false,"pulley");
    initPinOutput(GPIO_TO_PIN(2,3),false,"pulley");
    initPinOutput(GPIO_TO_PIN(2,4),false,"pulley");


}


void timer_handler(unsigned long arg)
{
    printk("GPIO_TO_PIN(2,1): %d\n",gpio_get_value(GPIO_TO_PIN(2,1)));
	unsigned long j = jiffies;
	s_timer.expires = j + HZ/100 ;
	add_timer(&s_timer);

}

static void initTimer(void)
{
   unsigned long j = jiffies;
   init_timer(&s_timer);
   s_timer.function = timer_handler;
   s_timer.expires = j + HZ;

   add_timer(&s_timer);

}


static int szhc_pulley_open(struct inode *inode, struct file *file)
{
    printk("szhc_pulley_open \n");
    return 0;
}

static int szhc_pulley_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    printk("**szhc_pulley_ioctl(arg=%d,cmd=%d)(/drivers/char/szhc_pulley_jixieshou.c )**\n",arg,cmd);

    return 0;
}

static struct file_operations szhc_pulley_fops =
{
    .owner = THIS_MODULE,
    .open = szhc_pulley_open,
    .unlocked_ioctl = szhc_pulley_ioctl,
    //.compat_ioctl = szhc_pulley_ioctl,
};
static char __initdata banner[] = "SZHCAM335x  PULLEY, (c) 2014 SZHC\n";
static struct class *pulley_class;

static int __init szhc_pulley_init(void)
{
    int ret;
    printk(banner);

    /*初始化pulley配置*/
    initConfig();

    /* 注册字符设备驱动程序
     * 参数为主设备号、设备名字、file_operations 结构;
     * 这样,主设备号就和具体的 file_operations 结构联系起来了,
     * 操作主设备为 pulley_MAJOR 的设备文件时,就会调用 EmbedSky_pulley_fops 中的相关成员函数
     * PULLEY_MAJOR 可以设为 0,表示由内核自动分配主设备号
     */
    ret = register_chrdev(PULLEY_MAJOR, DEVICE_NAME, &szhc_pulley_fops);
    if (ret < 0)
    {
        printk(DEVICE_NAME " can't register major number\n");
        return ret;
    }
    //注册一个类,使 mdev 可以在"/dev/"目录下面建立设备节点
    pulley_class = class_create(THIS_MODULE, DEVICE_NAME);
    if(IS_ERR(pulley_class))
    {
        printk("Err: faipulley in EmbedSky-pulley class. \n");
        return -1;
    }
    //创建一个设备节点,节点名为 DEVICE_NAME
    device_create(pulley_class, NULL, MKDEV(PULLEY_MAJOR, 0), NULL, DEVICE_NAME);

    /*初始化Timer配置*/
    initTimer();

    return 0;
}

static void __exit szhc_pulley_exit(void)
{
    /* 卸载驱动程序 */
  	del_timer(&s_timer);
    unregister_chrdev(PULLEY_MAJOR, DEVICE_NAME);
    device_destroy(pulley_class, MKDEV(PULLEY_MAJOR, 0)); //删掉设备节点
    class_destroy(pulley_class); 		//注销类
}

/* 这两行指定驱动程序的初始化函数和卸载函数 */
module_init(szhc_pulley_init);
module_exit(szhc_pulley_exit);

/* 描述驱动程序的一些信息,不是必须的 */
MODULE_AUTHOR("FANYUKUI in SZHC");// 驱动程序的作者
MODULE_DESCRIPTION("AM335x PULLEY Driver"); // 一些描述信息
MODULE_LICENSE("GPL");  // 遵循的协议

