/*************************************
NAME:szhc_single_leds.c
COPYRIGHT:SZHC
*************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/device.h>

/* 加载模式后,执行”cat /proc/devices”命令看到的设备名称 */
#define DEVICE_NAME "szhc_leds"
#define LED_MAJOR 232 /* 主设备号 */

/* 应用程序执行 ioctl(fd, cmd, arg)时的第 2 个参数 */
#define IOCTL_LED_ON 1
#define IOCTL_LED_OFF 0
#define IOCTL_LED_ALL 2


#define GPIO1					0x4804C000
#define GPIO_OE 					0x134
#define GPIO_DATAOUT			0x13c
#define GPIO_CLEARDATAOUT 		0x190
#define GPIO_SETDATAOUT		0x194
#define GPIO_CTRL				0x130
#define CONTROL_MODULE			0x44E10000
#define LED_MODE    0x07


typedef struct _ledConfig{
    volatile unsigned long *gpio_oe;
    volatile unsigned long *gpio_dataout;
    volatile unsigned long *gpio_setdataout;
    volatile unsigned long *gpio_cleardataout;
    volatile unsigned long *gpio_ctrl;
    volatile unsigned long *module_ctrl[4];
}LedConfig;

/* 用来指定 LED 片选引脚 */
static unsigned long ctrl_table [4] =
{
    0x814,
    0x818,
    0x81C,
    0x820,
};

/* 用来指定 LED 所用的 GPIO 引脚 */
static unsigned long led_table [] =
{
    21,
    22,
    23,
    24,
};


static LedConfig leds;

static void initLedConfig()
{
    int i;
    leds.gpio_oe = (volatile unsigned long *)ioremap(GPIO1 + GPIO_OE , sizeof(volatile unsigned long ));
    leds.gpio_dataout = (volatile unsigned long *)ioremap(GPIO1 + GPIO_DATAOUT, sizeof(volatile unsigned long ));
    leds.gpio_setdataout =( volatile unsigned long *)ioremap(GPIO1 + GPIO_SETDATAOUT, sizeof(volatile unsigned long ));
    leds.gpio_cleardataout = (volatile unsigned long *)ioremap(GPIO1 + GPIO_CLEARDATAOUT , sizeof(volatile unsigned long ));
    leds.gpio_ctrl=(volatile unsigned long *)ioremap(GPIO1+GPIO_CTRL,sizeof(volatile unsigned long ));
    for(i=0; i<4;i++)
    {
        leds.module_ctrl[i]  = (volatile unsigned long *)ioremap(CONTROL_MODULE+ctrl_table[i],sizeof(volatile unsigned long ));
    }
    /*
    *配置相关引脚的pin_mux模式；
    *配置相关寄存器；
    *首先是寄存器OE，输出使能；设置0为输出使能；
    *其次是寄存器SETDATAOUT，设置允许输出位；设置1为允许；
    *最后是寄存器DATAOUT，设置输出高低电平；
    */
    for(i =0;i<4;i++)
    {
        *(leds.module_ctrl[i]) |= LED_MODE;
    }
    unsigned long value =(0x1<<led_table[0])|(0x1<<led_table[1])|(0x1<<led_table[2])|(0x1<<led_table[3]);
    *(leds.gpio_ctrl) &=~(0x1);
    *(leds.gpio_oe) &= ~value;
    *(leds.gpio_dataout) |= value;
    *(leds.gpio_setdataout) |= value;
    printk("insmod the leds module!\n");

}

void set_led_value(int index,int value)
{
    if(value){
        *(leds.gpio_setdataout) |= (1 << index);
    }
    else{
        *(leds.gpio_cleardataout) |= (1 << index);
    }

}

static int szhc_leds_open(struct inode *inode, struct file *file)
{
    printk("szhc_leds_open \n");
    return 0;
}

static int szhc_leds_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    printk("**szhc_leds_ioctl(arg=%d,cmd=%d)(/drivers/char/szhc_leds_jixieshou.c )**\n",arg,cmd);

    (arg & 8)? set_led_value(led_table[3], IOCTL_LED_ON)
             : set_led_value(led_table[3], IOCTL_LED_OFF);
    (arg & 4)? set_led_value(led_table[2], IOCTL_LED_ON)
             : set_led_value(led_table[2],IOCTL_LED_OFF);
    (arg & 2)?  set_led_value(led_table[1], IOCTL_LED_ON)
              :  set_led_value(led_table[1], IOCTL_LED_OFF);
    (arg & 1)?  set_led_value(led_table[0], IOCTL_LED_ON)
              :  set_led_value(led_table[0], IOCTL_LED_OFF);
    return 0;
}

static struct file_operations szhc_leds_fops =
{
    .owner = THIS_MODULE,
    .open = szhc_leds_open,
    .unlocked_ioctl = szhc_leds_ioctl,
    //.compat_ioctl = szhc_leds_ioctl,
};
static char __initdata banner[] = "SZHCAM335x  LEDS, (c) 2014 SZHC\n";
static struct class *led_class;

static int __init szhc_leds_init(void)
{
    int ret,i;
    printk(banner);

    /*初始化LED配置*/
    initLedConfig();

    /* 注册字符设备驱动程序
     * 参数为主设备号、设备名字、file_operations 结构;
     * 这样,主设备号就和具体的 file_operations 结构联系起来了,
     * 操作主设备为 LED_MAJOR 的设备文件时,就会调用 EmbedSky_leds_fops 中的相关成员函数
     * LED_MAJOR 可以设为 0,表示由内核自动分配主设备号
     */
    ret = register_chrdev(LED_MAJOR, DEVICE_NAME, &szhc_leds_fops);
    if (ret < 0)
    {
        printk(DEVICE_NAME " can't register major number\n");
        return ret;
    }
    //注册一个类,使 mdev 可以在"/dev/"目录下面建立设备节点
    led_class = class_create(THIS_MODULE, DEVICE_NAME);
    if(IS_ERR(led_class))
    {
        printk("Err: failed in EmbedSky-leds class. \n");
        return -1;
    }
    //创建一个设备节点,节点名为 DEVICE_NAME
    device_create(led_class, NULL, MKDEV(LED_MAJOR, 0), NULL, DEVICE_NAME);

    return 0;
}

static void __exit szhc_leds_exit(void)
{
    /* 卸载驱动程序 */
    unregister_chrdev(LED_MAJOR, DEVICE_NAME);
    device_destroy(led_class, MKDEV(LED_MAJOR, 0)); //删掉设备节点
    class_destroy(led_class); 		//注销类
}

/* 这两行指定驱动程序的初始化函数和卸载函数 */
module_init(szhc_leds_init);
module_exit(szhc_leds_exit);

/* 描述驱动程序的一些信息,不是必须的 */
MODULE_AUTHOR("FANYUKUI in SZHC");// 驱动程序的作者
MODULE_DESCRIPTION("AM335x LED Driver"); // 一些描述信息
MODULE_LICENSE("GPL");  // 遵循的协议

