/************************************
NAME:szhc_knob.c
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
#include <asm/signal.h>
#include <linux/sched.h>
#include <linux/gpio.h>

#define DEVICE_NAME "szhc_knob"
#define KNOB_MAJOR 237			/* 主设备号 */

#define IO_COUNT 3
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

struct SZHC_Knob_dev
{
	wait_queue_head_t waitQueue;
	//struct fasync_struct *async_queue;
	struct timer_list s_timer;
	atomic_t keyValue;
	int isPressed;
};

static struct SZHC_Knob_dev *knob_dev;

static unsigned long ioTable[] = {

	GPIO_TO_PIN (2, 0),
	GPIO_TO_PIN (2, 5),
	GPIO_TO_PIN (2, 2),
};

static void timer_handler (unsigned long arg)
{
	int i;
	int leftMove = 2;
	int keyValue = 7;			/*0111B */
	for (i = 0; i != IO_COUNT; ++i)
	{
		if ((gpio_get_value (ioTable[i])) == 0)
		{
			keyValue &= (~(1 << leftMove));
		}
		--leftMove;
	}

	if (keyValue != knob_dev->keyValue.counter)
	{
		printk ("**atomic_set keyValue=%d**\n", keyValue);
		atomic_set (&knob_dev->keyValue, keyValue);
		knob_dev->isPressed = 1;
		wake_up (&knob_dev->waitQueue);
	}

	knob_dev->s_timer.expires = jiffies + 20 * HZ / 1000;
	add_timer (&knob_dev->s_timer);
}

static void initPinOut (unsigned int gpio, bool isOut, char *label,
	unsigned int high)
{
	int result;
	/* Allocating GPIOs and setting direction */
	result = gpio_request (gpio, label);	//usr1
	if (result != 0)
		printk ("gpio_request(%d) failed!\n", gpio);
	if (isOut)
	{
		result = gpio_direction_output (gpio, high);
		if (result != 0)
			printk ("gpio_direction(%d) failed!\n", gpio);

	}
	else
	{
		result = gpio_direction_input (gpio);
		if (result != 0)
			printk ("gpio_direction(%d) failed!\n", gpio);
	}

}

static int szhc_knob_open (struct inode *inode, struct file *file)
{
	/*旋钮 输入 */
	initPinOut (GPIO_TO_PIN (2, 0), false, "Knob", 0);	//停止
	initPinOut (GPIO_TO_PIN (2, 5), false, "Knob", 0);	//自动 低电平有效
	initPinOut (GPIO_TO_PIN (2, 2), false, "Knob", 0);	//手动

	init_timer (&knob_dev->s_timer);
	knob_dev->s_timer.function = timer_handler;
	knob_dev->s_timer.expires = jiffies + HZ;

	add_timer (&knob_dev->s_timer);
	return 0;
}

static int szhc_knob_close (struct inode *inode, struct file *file)
{
	del_timer (&knob_dev->s_timer);
	return 0;
}

static ssize_t szhc_knob_read (struct file *filp, char __user * buf,
	size_t count, loff_t * offp)
{
	char data[count];
	int keyValue;

	if (!knob_dev->isPressed)
	{
		if (filp->f_flags & O_NONBLOCK)
		{
			printk ("Cannot O_NONBLOCK Mode!\n");
			return -EAGAIN;
		}
		else
		{
			wait_event (knob_dev->waitQueue, knob_dev->isPressed);
		}
	}
	memset (data, 0, count);
	keyValue = atomic_read (&knob_dev->keyValue);
	switch (keyValue)
	{
	case 6:
		keyValue = 1;
		break;
	case 3:
		keyValue = 4;
		break;
	case 1:
		keyValue = 6;
		break;
	default:
		keyValue = 0;
		break;
	}
	if (keyValue)
	{
		int ret;

		ret = sprintf (data, "%d\n", keyValue);
		if (ret < 0)
		{
			return -1;
		}
		if (copy_to_user (buf, data, count) != 0)
		{
			return -EFAULT;
		}
	}
	knob_dev->isPressed = 0;
	return count;
}

static unsigned int szhc_knob_poll (struct file *filp, poll_table * wait)
{
	unsigned int mask = 0;
	poll_wait (filp, &knob_dev->waitQueue, wait);
	if (knob_dev->isPressed)
	{
		mask |= POLLIN | POLLRDNORM;
	}
	return mask;
}

static struct file_operations szhc_knob_fops = {
	.owner = THIS_MODULE,
	.open = szhc_knob_open,
	.release = szhc_knob_close,
	.read = szhc_knob_read,
	.poll = szhc_knob_poll,
	//.fasync = szhc_knob_fasync,
};

static char __initdata banner[] = "AM335x knob (c)2014 SZHC\n";
static struct class *knob_class;

static int __init szhc_knob_init (void)
{
	int ret;
	printk (banner);

	ret = register_chrdev (KNOB_MAJOR, DEVICE_NAME, &szhc_knob_fops);
	if (ret < 0)
	{
		printk (DEVICE_NAME " can't register major number\n");
		return ret;
	}

	/***********************************************************/
	knob_dev = kmalloc (sizeof (struct SZHC_Knob_dev), GFP_KERNEL);
	if (knob_dev == NULL)
	{
		unregister_chrdev (KNOB_MAJOR, DEVICE_NAME);
		return 0;
	}

	memset (knob_dev, 0, sizeof (struct SZHC_Knob_dev));
	atomic_set (&knob_dev->keyValue, 7);
	init_waitqueue_head (&knob_dev->waitQueue);
	knob_dev->isPressed = 0;
	/************************************************************/

	knob_class = class_create (THIS_MODULE, DEVICE_NAME);
	if (IS_ERR (knob_class))
	{
		printk ("Err: failed in SZHC knob class. \n");
		return -1;
	}
	//创建一个设备节点,节点名为 DEVICE_NAME
	device_create (knob_class, NULL, MKDEV (KNOB_MAJOR, 0), NULL, DEVICE_NAME);

	return 0;
}

static void __exit szhc_knob_exit (void)
{
	printk ("**begin exit(drivers/char/szhc_knob.c)**\n");
	kfree (knob_dev);
	unregister_chrdev (KNOB_MAJOR, DEVICE_NAME);
	device_destroy (knob_class, MKDEV (KNOB_MAJOR, 0));
	class_destroy (knob_class);
	printk ("**end exit(drivers/char/szhc_knob.c)**\n");
}

//删掉设备节点
/* 这两行指定驱动程序的初始化函数和卸载函数 */
module_init (szhc_knob_init);
module_exit (szhc_knob_exit);

/* 描述驱动程序的一些信息,不是必须的 */
MODULE_AUTHOR ("fanyukui in SZHC");	// 驱动程序的作者
MODULE_DESCRIPTION ("AM335x Knob Driver");	// 一些描述信息
MODULE_LICENSE ("GPL");			// 遵循的协议
