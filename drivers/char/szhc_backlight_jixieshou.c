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


/* ����ģʽ��,ִ�С�cat /proc/devices����������豸���� */
#define DEVICE_NAME "szhc_lcd_backlight"

#define LCD_MAJOR 233 /* ���豸�� */


/* Ӧ�ó���ִ�� ioctl(fd, cmd, arg)ʱ�ĵ� 2 ������ */
#define IOCTL_LCD_ON 1
#define IOCTL_LCD_OFF 0


#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))


/* ����ָ�� ���� ���õ� GPIO BANK */
#define BANK1  1

#define GPIO_NUMS 2

/* ����ָ�� ���� ���õ� GPIO ���� */
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

     /*��ʼ��LCD����*/
     //initLcdConfig();

	/* ע���ַ��豸��������
	 * ����Ϊ���豸�š��豸���֡�file_operations �ṹ;
	 * ����,���豸�žͺ;���� file_operations �ṹ��ϵ������,
	 * �������豸Ϊ LCD_MAJOR ���豸�ļ�ʱ,�ͻ���� EmbedSky_leds_fops �е���س�Ա����
	 * LED_MAJOR ������Ϊ 0,��ʾ���ں��Զ��������豸��
	 */
     ret = register_chrdev(LCD_MAJOR, DEVICE_NAME, &szhc_lcd_fops);
     if (ret < 0)
     {
				  printk(DEVICE_NAME " can't register major number\n");
	 				 return ret;
     }
		//ע��һ����,ʹ mdev ������"/dev/"Ŀ¼���潨���豸�ڵ�
     lcd_class = class_create(THIS_MODULE, DEVICE_NAME);
     if(IS_ERR(lcd_class))
     {
	       printk("Err: failed in EmbedSky-leds class. \n");
	       return -1;
     }
		//����һ���豸�ڵ�,�ڵ���Ϊ DEVICE_NAME
     device_create(lcd_class, NULL, MKDEV(LCD_MAJOR, 0), NULL, DEVICE_NAME);

     return 0;
}

static void __exit szhc_lcd_exit(void)
{
		/* ж���������� */
     unregister_chrdev(LCD_MAJOR, DEVICE_NAME);
     device_destroy(lcd_class, MKDEV(LCD_MAJOR, 0)); //ɾ���豸�ڵ�
     class_destroy(lcd_class); 		//ע����
}

/* ������ָ����������ĳ�ʼ��������ж�غ��� */
module_init(szhc_lcd_init);
module_exit(szhc_lcd_exit);

/* �������������һЩ��Ϣ,���Ǳ���� */
MODULE_AUTHOR("FANYUKUI in SZHC");// �������������
MODULE_DESCRIPTION("AM335x LCD BACKLIGHT Driver"); // һЩ������Ϣ
MODULE_LICENSE("GPL");  // ��ѭ��Э��



