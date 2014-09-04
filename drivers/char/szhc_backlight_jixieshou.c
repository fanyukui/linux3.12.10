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
#define DEVICE_NAME "szhc_lcd_backlight"

#define LCD_MAJOR 233 /* 主设备号 */


/* 应用程序执行 ioctl(fd, cmd, arg)时的第 2 个参数 */
#define IOCTL_LCD_ON 1
#define IOCTL_LCD_OFF 0


#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))


/* 用来指定 背光 所用的 GPIO BANK */
#define BANK1  1

#define GPIO_NUMS 2

/* 用来指定 背光 所用的 GPIO 引脚 */
static unsigned long gpio_table[GPIO_NUMS] = {18,19};

void initPin(unsigned int bank,unsigned int gpio)
{
    int result;
    /* Allocating GPIOs and setting direction */
    result = gpio_request(GPIO_TO_PIN(bank,gpio), "BackLight");//usr1
    if (result != 0)
        printk("gpio_request(%d_%d) failed!\n",bank,gpio);
    result = gpio_direction_output(GPIO_TO_PIN(bank,gpio), 1);
    if (result != 0)
        printk("gpio_direction(%d_%d) failed!\n",bank,gpio);
}

void initLcdConfig(void)
{
	int i;
	for(i=0;i<GPIO_NUMS;i++)
	{
	    initPin(BANK1,gpio_table[i]);
	}
}


void set_lcd_value(int index,int value)
{
	if(value){
    gpio_set_value(GPIO_TO_PIN(BANK1,index), 1);
	}
	else{
    gpio_set_value(GPIO_TO_PIN(BANK1,index), 0);
	}

}

static int szhc_lcd_open(struct inode *inode, struct file *file)
{
	 printk("szhc_lcdbacklight_open \n");
	 return 0;
}

static int szhc_lcd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	printk("**szhc_lcd_ioctl(arg=%d,cmd=%d)(/drivers/char/szhc_lcd_jixieshou.c )**\n",arg,cmd);
    int i;
    for(i=0; i<GPIO_NUMS;i++)
    {
    	(arg & (1 << i))? set_lcd_value(gpio_table[i], IOCTL_LCD_ON)
    		: set_lcd_value(gpio_table[i], IOCTL_LCD_OFF);

    }

	return 0;
}

static struct file_operations szhc_lcd_fops =
{
     .owner = THIS_MODULE,
     .open = szhc_lcd_open,
     .unlocked_ioctl = szhc_lcd_ioctl,
	 //.compat_ioctl = szhc_leds_ioctl,
};
static char __initdata banner[] = "SZHCAM335x  LCD BACKLIGHT, (c) 2014 SZHC\n";
static struct class *lcd_class;

static int __init szhc_lcd_init(void)
{
     int ret,i;
     printk(banner);

     /*初始化LCD配置*/
     //initLcdConfig();

	/* 注册字符设备驱动程序
	 * 参数为主设备号、设备名字、file_operations 结构;
	 * 这样,主设备号就和具体的 file_operations 结构联系起来了,
	 * 操作主设备为 LCD_MAJOR 的设备文件时,就会调用 EmbedSky_leds_fops 中的相关成员函数
	 * LED_MAJOR 可以设为 0,表示由内核自动分配主设备号
	 */
     ret = register_chrdev(LCD_MAJOR, DEVICE_NAME, &szhc_lcd_fops);
     if (ret < 0)
     {
				  printk(DEVICE_NAME " can't register major number\n");
	 				 return ret;
     }
		//注册一个类,使 mdev 可以在"/dev/"目录下面建立设备节点
     lcd_class = class_create(THIS_MODULE, DEVICE_NAME);
     if(IS_ERR(lcd_class))
     {
	       printk("Err: failed in EmbedSky-leds class. \n");
	       return -1;
     }
		//创建一个设备节点,节点名为 DEVICE_NAME
     device_create(lcd_class, NULL, MKDEV(LCD_MAJOR, 0), NULL, DEVICE_NAME);

     return 0;
}

static void __exit szhc_lcd_exit(void)
{
		/* 卸载驱动程序 */
     unregister_chrdev(LCD_MAJOR, DEVICE_NAME);
     device_destroy(lcd_class, MKDEV(LCD_MAJOR, 0)); //删掉设备节点
     class_destroy(lcd_class); 		//注销类
}

/* 这两行指定驱动程序的初始化函数和卸载函数 */
module_init(szhc_lcd_init);
module_exit(szhc_lcd_exit);

/* 描述驱动程序的一些信息,不是必须的 */
MODULE_AUTHOR("FANYUKUI in SZHC");// 驱动程序的作者
MODULE_DESCRIPTION("AM335x LCD BACKLIGHT Driver"); // 一些描述信息
MODULE_LICENSE("GPL");  // 遵循的协议



