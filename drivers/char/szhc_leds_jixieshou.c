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
#define DEVICE_NAME "szhc_leds"

#define LED_MAJOR 232 /* ���豸�� */

/* Ӧ�ó���ִ�� ioctl(fd, cmd, arg)ʱ�ĵ� 2 ������ */
#define IOCTL_LED_ON 1
#define IOCTL_LED_OFF 0
#define IOCTL_LED_ALL 2

#define LED_NUMS 4
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

/* ����ָ�� LED ���õ� GPIO ���� */
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
    result = gpio_direction_output(gpio, 0); //����
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

    /*��ʼ��LED����*/
    initLedConfig();

    /* ע���ַ��豸��������
         * ����Ϊ���豸�š��豸���֡�file_operations �ṹ;
         * ����,���豸�žͺ;���� file_operations �ṹ��ϵ������,
         * �������豸Ϊ LED_MAJOR ���豸�ļ�ʱ,�ͻ���� EmbedSky_leds_fops �е���س�Ա����
         * LED_MAJOR ������Ϊ 0,��ʾ���ں��Զ��������豸��
         */
    ret = register_chrdev(LED_MAJOR, DEVICE_NAME, &szhc_leds_fops);
    if (ret < 0)
    {
        printk(DEVICE_NAME " can't register major number\n");
        return ret;
    }
    //ע��һ����,ʹ mdev ������"/dev/"Ŀ¼���潨���豸�ڵ�
    led_class = class_create(THIS_MODULE, DEVICE_NAME);
    if(IS_ERR(led_class))
    {
        printk("Err: failed in EmbedSky-leds class. \n");
        return -1;
    }
    //����һ���豸�ڵ�,�ڵ���Ϊ DEVICE_NAME
    device_create(led_class, NULL, MKDEV(LED_MAJOR, 0), NULL, DEVICE_NAME);

    return 0;
}

static void __exit szhc_leds_exit(void)
{
    /* ж���������� */
    unregister_chrdev(LED_MAJOR, DEVICE_NAME);
    device_destroy(led_class, MKDEV(LED_MAJOR, 0)); //ɾ���豸�ڵ�
    class_destroy(led_class); 		//ע����
}

/* ������ָ����������ĳ�ʼ��������ж�غ��� */
module_init(szhc_leds_init);
module_exit(szhc_leds_exit);

/* �������������һЩ��Ϣ,���Ǳ���� */
MODULE_AUTHOR("FANYUKUI in SZHC");// �������������
MODULE_DESCRIPTION("AM335x LED Driver"); // һЩ������Ϣ
MODULE_LICENSE("GPL");  // ��ѭ��Э��


