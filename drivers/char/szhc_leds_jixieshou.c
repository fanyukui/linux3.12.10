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

/* ����ģʽ��,ִ�С�cat /proc/devices����������豸���� */
#define DEVICE_NAME "szhc_leds"
#define LED_MAJOR 232 /* ���豸�� */

/* Ӧ�ó���ִ�� ioctl(fd, cmd, arg)ʱ�ĵ� 2 ������ */
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

/* ����ָ�� LED Ƭѡ���� */
static unsigned long ctrl_table [4] =
{
    0x814,
    0x818,
    0x81C,
    0x820,
};

/* ����ָ�� LED ���õ� GPIO ���� */
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
    *����������ŵ�pin_muxģʽ��
    *������ؼĴ�����
    *�����ǼĴ���OE�����ʹ�ܣ�����0Ϊ���ʹ�ܣ�
    *����ǼĴ���SETDATAOUT�������������λ������1Ϊ����
    *����ǼĴ���DATAOUT����������ߵ͵�ƽ��
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

