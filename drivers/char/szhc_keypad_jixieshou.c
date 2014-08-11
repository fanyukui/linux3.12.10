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



/* ����ģʽ��,ִ�С�cat /proc/devices����������豸���� */
#define DEVICE_NAME "szhc_keypads"

#define KEYPAD_MAJOR 235 /* ���豸�� */


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
	initPin(0,13); //ֹͣ
	initPin(2,5);  //�Զ� �͵�ƽ��Ч
	initPin(2,2);  //�ֶ�

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

    /*��ʼ��LED����*/
    initConfig();

    /*��ʼ��Timer����*/
    initTimer();

    /* ע���ַ��豸��������
     * ����Ϊ���豸�š��豸���֡�file_operations �ṹ;
     * ����,���豸�žͺ;���� file_operations �ṹ��ϵ������,
     * �������豸Ϊ LED_MAJOR ���豸�ļ�ʱ,�ͻ���� EmbedSky_leds_fops �е���س�Ա����
     * LED_MAJOR ������Ϊ 0,��ʾ���ں��Զ��������豸��
     */
    ret = register_chrdev(KEYPAD_MAJOR, DEVICE_NAME, &szhc_keypad_fops);
    if (ret < 0)
    {
        printk(DEVICE_NAME " can't register major number\n");
        return ret;
    }
    //ע��һ����,ʹ mdev ������"/dev/"Ŀ¼���潨���豸�ڵ�
    keypad_class = class_create(THIS_MODULE, DEVICE_NAME);
    if(IS_ERR(keypad_class))
    {
        printk("Err: failed in EmbedSky-leds class. \n");
        return -1;
    }
    //����һ���豸�ڵ�,�ڵ���Ϊ DEVICE_NAME
    device_create(keypad_class, NULL, MKDEV(KEYPAD_MAJOR, 0), NULL, DEVICE_NAME);

    return 0;
}

static void __exit szhc_keypad_exit(void)
{
    /* ж���������� */
    unregister_chrdev(KEYPAD_MAJOR, DEVICE_NAME);
    device_destroy(keypad_class, MKDEV(KEYPAD_MAJOR, 0)); //ɾ���豸�ڵ�
    class_destroy(keypad_class); 		//ע����
}

/* ������ָ����������ĳ�ʼ��������ж�غ��� */
module_init(szhc_keypad_init);
module_exit(szhc_keypad_exit);

/* �������������һЩ��Ϣ,���Ǳ���� */
MODULE_AUTHOR("FANYUKUI in SZHC");// �������������
MODULE_DESCRIPTION("AM335x LED Driver"); // һЩ������Ϣ
MODULE_LICENSE("GPL");  // ��ѭ��Э��
