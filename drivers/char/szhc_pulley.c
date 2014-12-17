/************************************
NAME:szhc_pulley.c
COPYRIGHT:SZHC
*************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <mach/hardware.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/poll.h>
#include <asm/atomic.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/gpio.h>

#define DEVICE_NAME "szhc_pulley"
#define pulley_MAJOR 240		/* 主设备号 */
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))
#define PULLEY_DELAY    600



struct SZHC_Pulley_dev
{
	wait_queue_head_t waitQueue;
  	int isChanged;
	atomic_t data;
};



static struct SZHC_Pulley_dev *pulley_dev;
static char __initdata banner[] = "AM335x pulley (c)2014 SZHC\n";
static struct class *pulley_class;

static int szhc_pulley_open (struct inode *inode, struct file *file)
{
	//do nothing
	return 0;
}

static int szhc_pulley_close (struct inode *inode, struct file *file)
{
	//do nothing
	return 0;
}

static ssize_t szhc_pulley_read (struct file *filp, char __user * buf, size_t count, loff_t * offp)
{
    char temp;

	if (filp->f_flags & O_NONBLOCK)
	{
		return -EAGAIN;
	}
	else
	{
		wait_event (pulley_dev->waitQueue, pulley_dev->isChanged);
	}

    temp = atomic_read(&pulley_dev->data);
    if (copy_to_user (buf, &temp,sizeof(temp))!= 0)
	{
	    printk("copy to user failed\n");
		return -EFAULT;
	}

    pulley_dev->isChanged = 0;

    return sizeof(temp);
}

irqreturn_t irq_handler (int irqno, void *dev_id)
{

	if (gpio_get_value (GPIO_TO_PIN(2,4)))
	{
		atomic_set(&pulley_dev->data ,1);
        printk("****UP****\n");
	}
	else
	{
        atomic_set(&pulley_dev->data ,-1);
        printk("****DOWN****\n");
	}
    pulley_dev->isChanged = 1;
    wake_up(&pulley_dev->waitQueue);
	return IRQ_HANDLED;
}

static struct file_operations szhc_pulley_fops = {
	.owner = THIS_MODULE,
	.open = szhc_pulley_open,
	.release = szhc_pulley_close,
	.read = szhc_pulley_read,
};

static void initPinOut(unsigned int gpio,bool isOut,char *label,unsigned int high)
{
    int result;
    /* Allocating GPIOs and setting direction */
    result = gpio_request(gpio, label);//usr1
    if (result != 0)
        printk("gpio_request(%d) failed!\n",gpio);
    if(isOut){
        result = gpio_direction_output(gpio,high);
        if (result != 0)
            printk("gpio_direction(%d) failed!\n",gpio);

    }
    else{
        result = gpio_direction_input(gpio);
        if (result != 0)
            printk("gpio_direction(%d) failed!\n",gpio);
    }

}
static void initConfig (void)
{
	int irq, ret;

     /*滚轮 输入*/
    initPinOut(GPIO_TO_PIN(2,1),false,"pulley",0);
    initPinOut(GPIO_TO_PIN(2,3),false,"pulley",0);  /*中断口*/
    initPinOut(GPIO_TO_PIN(2,4),false,"pulley",0);

	gpio_set_debounce (GPIO_TO_PIN(2,3), PULLEY_DELAY);

	/*申请中断 */
	irq = gpio_to_irq (GPIO_TO_PIN(2,3));
	if (irq < 0)
	{
		printk ("GPIO_TO_PIN(2, 3) irq  failed !\n");
		ret = irq;
	}
	ret = request_irq (irq, irq_handler, IRQF_TRIGGER_FALLING | IRQF_SHARED, "pulley", irq_handler);
	if (ret)
	{
		printk ("request_irq failed ! %d\n", ret);
	}

}

static int __init szhc_pulley_init (void)
{
	int ret;
	printk (banner);

	ret = register_chrdev (pulley_MAJOR, DEVICE_NAME, &szhc_pulley_fops);
	if (ret < 0)
	{
		printk (DEVICE_NAME " can't register major number\n");
		return ret;
	}

	pulley_dev = kmalloc (sizeof (struct SZHC_Pulley_dev), GFP_KERNEL);
	if (pulley_dev == NULL)
	{
		unregister_chrdev (pulley_MAJOR, DEVICE_NAME);
		return 0;
	}

	memset (pulley_dev, 0, sizeof (struct SZHC_Pulley_dev));
	init_waitqueue_head (&pulley_dev->waitQueue);

	pulley_class = class_create (THIS_MODULE, DEVICE_NAME);
	if (IS_ERR (pulley_class))
	{
		printk ("Err: failed in SZHC pulley class. \n");
		return -1;
	}
	//创建一个设备节点,节点名为 DEVICE_NAME
	device_create (pulley_class, NULL, MKDEV (pulley_MAJOR, 0), NULL, DEVICE_NAME);

	initConfig ();
	return 0;
}

static void __exit szhc_pulley_exit (void)
{
	kfree (pulley_dev);
	unregister_chrdev (pulley_MAJOR, DEVICE_NAME);
	device_destroy (pulley_class, MKDEV (pulley_MAJOR, 0));
	class_destroy (pulley_class);
}

//删掉设备节点
/* 这两行指定驱动程序的初始化函数和卸载函数 */
module_init (szhc_pulley_init);
module_exit (szhc_pulley_exit);

/* 描述驱动程序的一些信息,不是必须的 */
MODULE_AUTHOR ("Fanyukui in SZHC");	// 驱动程序的作者
MODULE_DESCRIPTION ("AM335x pulley Driver");	// 一些描述信息
MODULE_LICENSE ("GPL");			// 遵循的协议
