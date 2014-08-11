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

#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

/* ����ָ�� LED ���õ� GPIO ���� */
static unsigned long led_table [4] ={ 21,22,23,24};

void initPin(unsigned int bank,unsigned int gpio)
{
    int result;
    /* Allocating GPIOs and setting direction */
    result = gpio_request(GPIO_TO_PIN(bank,gpio), "Leds");//usr1
    if (result != 0)
        printk("gpio_request(%d_%d) failed!\n",bank,gpio);
    result = gpio_direction_output(GPIO_TO_PIN(bank,gpio), 1);
    if (result != 0)
        printk("gpio_direction(%d_%d) failed!\n",bank,gpio);
}

void initLedConfig(void)
{
    int i;
    for(i=0;i<4;i++)
    {
        initPin(1,led_table[i]);
    }
}


void set_led_value(int index,int value)
{
    if(value){
        gpio_set_value(GPIO_TO_PIN(1,index), 1);
    }
    else{
        gpio_set_value(GPIO_TO_PIN(1,index), 0);
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


