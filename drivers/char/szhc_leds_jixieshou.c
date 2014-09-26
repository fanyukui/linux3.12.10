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
#include <linux/leds.h>
#include <linux/device.h>
#include <linux/gpio.h>


/* 加载模式后,执行”cat /proc/devices”命令看到的设备名称 */
#define DEVICE_NAME "szhc_leds"

#define LED_MAJOR 232 /* 主设备号 */

/* 应用程序执行 ioctl(fd, cmd, arg)时的第 2 个参数 */
#define IOCTL_LED_ON 1
#define IOCTL_LED_OFF 0
#define IOCTL_LED_ALL 2

#define LED_NUMS 4
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

/* 用来指定 LED 所用的 GPIO 引脚 */
static unsigned long led_table [LED_NUMS] ={
    GPIO_TO_PIN(1,21),
    GPIO_TO_PIN(1,22),
    GPIO_TO_PIN(1,23),
    GPIO_TO_PIN(1,24)
};

void initPin(unsigned int gpio)
{
    int result;
    /* Allocating GPIOs and setting direction */
    result = gpio_request(gpio, "Leds");//usr1
    if (result != 0)
        printk("gpio_request %d failed!\n",gpio);
    result = gpio_direction_output(gpio, 0); //拉低
    if (result != 0)
        printk("gpio_direction %d failed!\n",gpio);
}

void initLedConfig(void)
{
    int i;
    for(i=0;i<LED_NUMS;i++)
    {
        initPin(led_table[i]);
    }
}

static int szhc_leds_open(struct inode *inode, struct file *file)
{
    printk("szhc_leds_open \n");
    return 0;
}

static long szhc_leds_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    //printk("**szhc_leds_ioctl(arg=%u,cmd=%u)(/drivers/char/szhc_leds_jixieshou.c )**\n",arg,cmd);
    int i;
    for(i = 0; i < LED_NUMS;i ++)    {
         (arg & (1 << i))  ?  gpio_set_value(led_table[i], IOCTL_LED_ON)
                    :  gpio_set_value(led_table[i], IOCTL_LED_OFF);
    }
    return 0;
}

static struct file_operations szhc_leds_fops =
{
    .owner = THIS_MODULE,
    .open = szhc_leds_open,
    .unlocked_ioctl = szhc_leds_ioctl
    //.compat_ioctl = szhc_leds_ioctl,
};
static char __initdata banner[] = "SZHCAM335x  LEDS, (c) 2014 SZHC\n";
static struct class *led_class;

static int __init szhc_leds_init(void)
{
    int ret;
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


