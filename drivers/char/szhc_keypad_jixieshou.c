/*************************************
NAME:szhc_keypad_jixieshou.c
COPYRIGHT:SZHC
*************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#include <asm/atomic.h>
#include <linux/gpio.h>



/* 加载模式后,执行”cat /proc/devices”命令看到的设备名称 */
#define DEVICE_NAME "szhc_keypads"

#define KEYPAD_MAJOR 235 /* 主设备号 */


#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

struct timer_list s_timer;

void initPin(unsigned int bank,unsigned int gpio)
{
    int result;
    /* Allocating GPIOs and setting direction */
    result = gpio_request(GPIO_TO_PIN(bank,gpio), "Keypad");//usr1
    if (result != 0)
        printk("gpio_request(%d_%d) failed!\n",bank,gpio);
    result = gpio_direction_input(GPIO_TO_PIN(bank,gpio));
    if (result != 0)
        printk("gpio_direction(%d_%d) failed!\n",bank,gpio);
}

void initConfig(void)
{
	initPin(0,13); //停止
	initPin(2,5);  //自动 低电平有效
	initPin(2,2);  //手动

}
void timer_handler(unsigned long arg)
{
	printk("GPIO_TO_PIN(0,13): %d\n",gpio_get_value(GPIO_TO_PIN(0,13)));
  	printk("GPIO_TO_PIN(2,5) : %d\n",gpio_get_value(GPIO_TO_PIN(2,5)));
	printk("GPIO_TO_PIN(2,2) : %d\n",gpio_get_value(GPIO_TO_PIN(2,2)));

	unsigned long j = jiffies;
	s_timer.expires = j + HZ ;
	add_timer(&s_timer);
}


void initTimer(void)
{
   unsigned long j = jiffies;
   init_timer(&s_timer);
   s_timer.function = timer_handler;
   s_timer.expires = j + HZ;

   add_timer(&s_timer);

}




static int szhc_keypad_open(struct inode *inode, struct file *file)
{
    printk("szhc_leds_open \n");
    return 0;
}



static struct file_operations szhc_keypad_fops =
{
    .owner = THIS_MODULE,
    .open = szhc_keypad_open,
    //.compat_ioctl = szhc_leds_ioctl,
};
static char __initdata banner[] = "SZHCAM335x  Keypad, (c) 2014 SZHC\n";
static struct class *keypad_class;

static int __init szhc_keypad_init(void)
{
    int ret,i;
    printk(banner);

    /*初始化LED配置*/
    initConfig();

    /*初始化Timer配置*/
    initTimer();

    /* 注册字符设备驱动程序
     * 参数为主设备号、设备名字、file_operations 结构;
     * 这样,主设备号就和具体的 file_operations 结构联系起来了,
     * 操作主设备为 LED_MAJOR 的设备文件时,就会调用 EmbedSky_leds_fops 中的相关成员函数
     * LED_MAJOR 可以设为 0,表示由内核自动分配主设备号
     */
    ret = register_chrdev(KEYPAD_MAJOR, DEVICE_NAME, &szhc_keypad_fops);
    if (ret < 0)
    {
        printk(DEVICE_NAME " can't register major number\n");
        return ret;
    }
    //注册一个类,使 mdev 可以在"/dev/"目录下面建立设备节点
    keypad_class = class_create(THIS_MODULE, DEVICE_NAME);
    if(IS_ERR(keypad_class))
    {
        printk("Err: failed in EmbedSky-leds class. \n");
        return -1;
    }
    //创建一个设备节点,节点名为 DEVICE_NAME
    device_create(keypad_class, NULL, MKDEV(KEYPAD_MAJOR, 0), NULL, DEVICE_NAME);

    return 0;
}

static void __exit szhc_keypad_exit(void)
{
    /* 卸载驱动程序 */
    unregister_chrdev(KEYPAD_MAJOR, DEVICE_NAME);
    device_destroy(keypad_class, MKDEV(KEYPAD_MAJOR, 0)); //删掉设备节点
    class_destroy(keypad_class); 		//注销类
}

/* 这两行指定驱动程序的初始化函数和卸载函数 */
module_init(szhc_keypad_init);
module_exit(szhc_keypad_exit);

/* 描述驱动程序的一些信息,不是必须的 */
MODULE_AUTHOR("FANYUKUI in SZHC");// 驱动程序的作者
MODULE_DESCRIPTION("AM335x LED Driver"); // 一些描述信息
MODULE_LICENSE("GPL");  // 遵循的协议
