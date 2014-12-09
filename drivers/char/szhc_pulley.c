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
#define pulley_MAJOR 240		/* ���豸�� */
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))
#define PULLEY_DELAY 200


typedef struct _PULLEY_DATA{
    atomic_t  forward;;
    atomic_t  count;
} PULLEY_DATA;

typedef struct _PULLEY_TEMP{
    int  forward;;
    int  count;
} PULLEY_TEMP;

struct SZHC_Pulley_dev
{
	wait_queue_head_t waitQueue;
  	int isChanged;
	PULLEY_DATA data;
};



static struct SZHC_Pulley_dev *pulley_dev;
static char __initdata banner[] = "AM335x pulley (c)2014 SZHC\n";
static struct class *pulley_class;
static int cycleCount = 0;
static int cycleFlag = 0;

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
    PULLEY_TEMP temp;

	if (filp->f_flags & O_NONBLOCK)
	{
		return -EAGAIN;
	}
	else
	{
		wait_event (pulley_dev->waitQueue, pulley_dev->isChanged);
	}

    temp.count = atomic_read(&pulley_dev->data.count);
    temp.forward = atomic_read(&pulley_dev->data.forward);

    if (copy_to_user (buf, &temp,sizeof(temp))!= 0)
	{
		return -EFAULT;
	}
   	atomic_set(&pulley_dev->data.count ,0);

    return sizeof(PULLEY_TEMP);
}

irqreturn_t irq_handler (int irqno, void *dev_id)
{

	if (gpio_get_value (GPIO_TO_PIN (2, 4)))
	{
	    if(cycleFlag){
            cycleCount ++;
        }
        else{
            cycleCount = 1;
        }
		atomic_set(&pulley_dev->data.forward ,1);
   		atomic_set(&pulley_dev->data.count ,cycleCount);

   	    cycleFlag  = 1;

	}
	else
	{
		if( cycleFlag == 0 ){
            cycleCount ++;
        }
        else{
            cycleCount = 1;
        }
		atomic_set(&pulley_dev->data.forward ,0);
   		atomic_set(&pulley_dev->data.count ,cycleCount);
	    cycleFlag  = 0;
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

static void initConfig (void)
{
	int irq, ret;
	gpio_set_debounce (GPIO_TO_PIN (2, 3), PULLEY_DELAY);

	/*�����ж� */
	irq = gpio_to_irq (GPIO_TO_PIN (2, 3));
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
	//����һ���豸�ڵ�,�ڵ���Ϊ DEVICE_NAME
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

//ɾ���豸�ڵ�
/* ������ָ����������ĳ�ʼ��������ж�غ��� */
module_init (szhc_pulley_init);
module_exit (szhc_pulley_exit);

/* �������������һЩ��Ϣ,���Ǳ���� */
MODULE_AUTHOR ("Fanyukui in SZHC");	// �������������
MODULE_DESCRIPTION ("AM335x pulley Driver");	// һЩ������Ϣ
MODULE_LICENSE ("GPL");			// ��ѭ��Э��
