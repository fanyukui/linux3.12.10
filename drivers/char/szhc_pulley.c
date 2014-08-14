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


/* ����ģʽ��,ִ�С�cat /proc/devices����������豸���� */
#define DEVICE_NAME "szhc_pulley"
#define PULLEY_MAJOR 235 /* ���豸�� */



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
    MUX_EVM();  /*���ÿ���ģ��*/
    iounmap(p);

    /*����I/O����*/
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

    /*��ʼ��pulley����*/
    initConfig();

    /* ע���ַ��豸��������
     * ����Ϊ���豸�š��豸���֡�file_operations �ṹ;
     * ����,���豸�žͺ;���� file_operations �ṹ��ϵ������,
     * �������豸Ϊ pulley_MAJOR ���豸�ļ�ʱ,�ͻ���� EmbedSky_pulley_fops �е���س�Ա����
     * PULLEY_MAJOR ������Ϊ 0,��ʾ���ں��Զ��������豸��
     */
    ret = register_chrdev(PULLEY_MAJOR, DEVICE_NAME, &szhc_pulley_fops);
    if (ret < 0)
    {
        printk(DEVICE_NAME " can't register major number\n");
        return ret;
    }
    //ע��һ����,ʹ mdev ������"/dev/"Ŀ¼���潨���豸�ڵ�
    pulley_class = class_create(THIS_MODULE, DEVICE_NAME);
    if(IS_ERR(pulley_class))
    {
        printk("Err: faipulley in EmbedSky-pulley class. \n");
        return -1;
    }
    //����һ���豸�ڵ�,�ڵ���Ϊ DEVICE_NAME
    device_create(pulley_class, NULL, MKDEV(PULLEY_MAJOR, 0), NULL, DEVICE_NAME);

    /*��ʼ��Timer����*/
    initTimer();

    return 0;
}

static void __exit szhc_pulley_exit(void)
{
    /* ж���������� */
  	del_timer(&s_timer);
    unregister_chrdev(PULLEY_MAJOR, DEVICE_NAME);
    device_destroy(pulley_class, MKDEV(PULLEY_MAJOR, 0)); //ɾ���豸�ڵ�
    class_destroy(pulley_class); 		//ע����
}

/* ������ָ����������ĳ�ʼ��������ж�غ��� */
module_init(szhc_pulley_init);
module_exit(szhc_pulley_exit);

/* �������������һЩ��Ϣ,���Ǳ���� */
MODULE_AUTHOR("FANYUKUI in SZHC");// �������������
MODULE_DESCRIPTION("AM335x PULLEY Driver"); // һЩ������Ϣ
MODULE_LICENSE("GPL");  // ��ѭ��Э��

