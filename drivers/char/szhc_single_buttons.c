/************************************
NAME:szhc_single_buttons.c
本文件用于实现IndustrialControlSystmRB1.0的硬键盘驱动
COPYRIGHT:SZHC
*************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <mach/hardware.h>
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
#include <linux/sched.h>
#include <asm/io.h>

#define DEVICE_NAME "szhc_keyboard"
#define SZHC_BUTTON_MAJOR 235	/* 主设备号 */

//#define ADD_BUZZER_WHIT_BUTTON

#define ColumnCount   6
#define RowCount   5

#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))
#define MODE7 7

/*I/O状态配置*/
#define IEN (1 << 5)
#define PD (0 << 3)
#define PU (2 << 3)
#define OFF (1 << 3)
#define GPIO_MOD_CTRL_BIT	BIT(0)

/*Module Control 寄存器*/
#define AM335X_CTRL_BASE			0x44E10000

#define GPIO_1_29        0x087C
#define GPIO_3_21        0x09AC
#define GPIO_3_7         0x09E4
#define GPIO_3_8         0x09E8
#define GPIO_3_18        0x09A0
#define GPIO_0_19        0x09B0
#define GPIO_0_18        0x0A1C
#define GPIO_3_13        0x0A34

/*模式配置*/
#define MUX_VAL(OFFSET,VALUE)\
    writel((VALUE) | (readl(p+(OFFSET)) & ~0x7), p + (OFFSET));

#define MUX_READ(OFFSET)    \
    readl(p+(OFFSET))

/*模式配置*/
#define MUX_EVM() \
    MUX_VAL(GPIO_1_29, (IEN | PU | MODE7 )) \
    MUX_VAL(GPIO_3_21, (IEN | PU | MODE7 )) \
    MUX_VAL(GPIO_3_7, (IEN | PU | MODE7 )) \
    MUX_VAL(GPIO_3_8, (IEN | PU | MODE7 )) \
    MUX_VAL(GPIO_3_18, (IEN | PU | MODE7 )) \
    MUX_VAL(GPIO_0_19, (IEN | PU | MODE7 ))   \
    MUX_VAL(GPIO_0_18, (IEN | PU | MODE7 ))  \
    MUX_VAL(GPIO_3_13, (IEN | PU | MODE7 ))


#define ADD_BUZZER_WHIT_BUTTON

static char keypad[RowCount][ColumnCount];
static int check_times[RowCount][ColumnCount];
static int rowNum, colomnNum;

static unsigned int columtable[ColumnCount] = {
	GPIO_TO_PIN (1, 27),
	GPIO_TO_PIN (1, 26),
	GPIO_TO_PIN (1, 25),
	GPIO_TO_PIN (0, 31),
	GPIO_TO_PIN (1, 17),
	GPIO_TO_PIN (0, 19)			//第六行
};

static unsigned int rowtable[RowCount] = {
	GPIO_TO_PIN (3, 8),
	GPIO_TO_PIN (3, 7),
	GPIO_TO_PIN (1, 29),
	GPIO_TO_PIN (3, 21),
	GPIO_TO_PIN (3, 18)
};

struct SZHC_Buttons_dev
{
	wait_queue_head_t waitQueue;
	atomic_t keyValue;
	struct cdev cdev;
	int isPressed;
	struct timer_list s_timer;
#ifdef ADD_BUZZER_WHIT_BUTTON
	struct timer_list b_timer;
#endif
};


void timer_handler (unsigned long);


struct SZHC_Buttons_dev *szhc_buttons_dev;
static int buzzer = 0;
static char __initdata banner[] = "SZHCAM335x  SingleButton, (c) 2014 SZHC\n";

#define Beep_On()   do {gpio_set_value(GPIO_TO_PIN(1,19), 1); } while(0)
#define Beep_Off()  do {gpio_set_value(GPIO_TO_PIN(1,19), 0); } while(0)
#define Do_Nothing()   do { } while (0)
#define KEYBOARD_DEBUG

#ifdef ADD_BUZZER_WHIT_BUTTON

void timer_handler_button (unsigned long arg)
{
	Beep_Off ();				//close the buzzer
	buzzer = 0;
}

void beep_once (void)
{
	if (buzzer == 0)
	{
		buzzer = 1;
		Beep_On ();
		szhc_buttons_dev->b_timer.expires = jiffies + 1 * HZ / 10;	//需要延时多久    这里为0.1秒
		add_timer (&szhc_buttons_dev->b_timer);
	}
}
#endif

void initPinOut (unsigned int gpio, bool isOut, char *label, unsigned int high)
{
	int result;
	/* Allocating GPIOs and setting direction */
	result = gpio_request (gpio, label);	//usr1
	if (result != 0)
		printk ("gpio_request(%d) failed!\n", gpio);
	if (isOut)
	{
		result = gpio_direction_output (gpio, high);	//beep 拉低
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

static void initKeyPad (void)
{
	int start = 2, i, j;
	for (i = 0; i < RowCount; i++)
	{
		for (j = 0; j < ColumnCount; j++)
		{
			keypad[i][j] = start++;
		}

	}
}

static void initConfig (void)
{
	int row, column;

	/*配置模式 */
	void __iomem *p = ioremap_nocache (AM335X_CTRL_BASE, SZ_4K);
	if (!p)
	{
		printk ("ioremap failed\n");
		return;
	}
	MUX_EVM ();
	iounmap (p);

	/*蜂鸣器 输出 */
	initPinOut (GPIO_TO_PIN (1, 19), true, "Beep", 0);

	for (row = 0; row < RowCount; row++)
	{
		initPinOut (rowtable[row], false, "KeyPadInput", 0);
	}

	for (column = 0; column < ColumnCount; column++)
	{
		initPinOut (columtable[column], true, "KeyPadOutput", 1);
	}

	/*初始化KeyPad值 */
	initKeyPad ();

    //初始化定时器
    init_timer (&szhc_buttons_dev->s_timer);
	szhc_buttons_dev->s_timer.function = timer_handler;
	szhc_buttons_dev->s_timer.expires = jiffies + HZ;

	add_timer (&szhc_buttons_dev->s_timer);

}

/*拉低指定列*/
void downColumn (int index)
{
	int i;
	for (i = 0; i < ColumnCount; i++)
	{
		if (i == index)
			gpio_set_value (columtable[i], 0);
		else
			gpio_set_value (columtable[i], 1);
	}

}

void timer_handler (unsigned long arg)
{

	int col = 0, row = 0;
	for (col = 0; col < ColumnCount; col++)
	{
		downColumn (col);
		for (row = 0; row < RowCount; row++)
		{
			if (gpio_get_value (rowtable[row]) == 0
               && !szhc_buttons_dev->isPressed)
			{
				check_times[row][col]++;
				if (check_times[row][col] >= 2)
				{
					Beep_On ();
					atomic_set (&szhc_buttons_dev->keyValue, keypad[row][col]);
					szhc_buttons_dev->isPressed = 1;
                    wake_up(&szhc_buttons_dev->waitQueue);
					check_times[row][col] = 0;
					rowNum = row;
					colomnNum = col;
#ifdef KEYBOARD_DEBUG
					printk ("row: %d\tcolumn:%d\tkey value: %3d \t pressed!\n",
                           row, col, keypad[row][col]);
#endif
				}
				goto finished;
				//input_sync(button_dev);
			}
			if (col == colomnNum
               && row == rowNum
               && gpio_get_value (rowtable[rowNum])
               && szhc_buttons_dev->isPressed)
			{
#ifdef KEYBOARD_DEBUG
				printk ("row: %d\tcolumn:%d\tkey value: %3d \t released!\n",
                        row, col, keypad[row][col]);
#endif
				szhc_buttons_dev->isPressed = 0;
				Beep_Off ();
				goto finished;

			}
		}

	}

	for (row = 0; row < RowCount; row++)
		for (col = 0; col < ColumnCount; col++)
		{
			check_times[row][col] = 0;
		}
  finished:
	Do_Nothing ();
	szhc_buttons_dev->s_timer.expires = jiffies + 20 * HZ / 1000;
	add_timer (&szhc_buttons_dev->s_timer);
}

int SZHC_Buttons_open (struct inode *inode, struct file *file)
{

	return 0;
}

int SZHC_Buttons_release (struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t SZHC_Buttons_read (struct file * filp, char __user * buf, size_t count, loff_t * f_pos)
{
	char data[count];
	int ret, keyValue;

	if (!szhc_buttons_dev->isPressed)
	{
		if (filp->f_flags & O_NONBLOCK)
		{
			printk ("**SZHC_Button Cann not O_NONBLOCK**\n");
			return -EAGAIN;
		}
		else
		{
			wait_event (szhc_buttons_dev->waitQueue, szhc_buttons_dev->isPressed);
		}
	}
	memset (data, 0, count);
	keyValue = atomic_read (&szhc_buttons_dev->keyValue);
	ret = sprintf (data, "%d", keyValue);
	if (ret < 0)
	{
		return -1;
	}
	if (copy_to_user (buf, data, count) != 0)
	{
		return -EFAULT;
	}
//	szhc_buttons_dev->isPressed = 0;
	return count;
}

static unsigned int SZHC_Buttons_poll (struct file *filp, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	poll_wait (filp, &szhc_buttons_dev->waitQueue, wait);
	if (szhc_buttons_dev->isPressed)
	{
		mask |= POLLIN | POLLRDNORM;
	}
	return mask;
}

static struct file_operations SZHC_Buttons_fops = {
	.owner = THIS_MODULE,
	.open = SZHC_Buttons_open,
	.release = SZHC_Buttons_release,
	.read = SZHC_Buttons_read,
	.poll = SZHC_Buttons_poll,
//  .fasync = SZHC_Buttons_fasync,
};

static struct class *szhc_buttons_class;
static int __init SZHC_Buttons_init (void)
{
	int ret;
	printk (banner);
	ret = register_chrdev (SZHC_BUTTON_MAJOR, DEVICE_NAME, &SZHC_Buttons_fops);
	if (ret < 0)
	{
		printk ("can't register szhc-buttons\n");
		return ret;
	}
	szhc_buttons_dev = kmalloc (sizeof (struct SZHC_Buttons_dev), GFP_KERNEL);
	if (szhc_buttons_dev == NULL)
	{
		ret = -ENOMEM;
		unregister_chrdev (SZHC_BUTTON_MAJOR, DEVICE_NAME);
		return ret;
	}

	memset (szhc_buttons_dev, 0, sizeof (struct SZHC_Buttons_dev));
	init_waitqueue_head (&szhc_buttons_dev->waitQueue);
	szhc_buttons_dev->isPressed = 0;
	atomic_set (&szhc_buttons_dev->keyValue, 0);

	szhc_buttons_class = class_create (THIS_MODULE, DEVICE_NAME);
	if (IS_ERR (szhc_buttons_class))
	{
		printk ("Err:failed in szhc_buttons_class.\n");
		return -1;
	}
	device_create (szhc_buttons_class, NULL, MKDEV (SZHC_BUTTON_MAJOR, 0), NULL, DEVICE_NAME);

	initConfig ();
#ifdef ADD_BUZZER_WHIT_BUTTON
	init_timer (&szhc_buttons_dev->b_timer);
	szhc_buttons_dev->b_timer.function = timer_handler_button;
#endif

	return ret;
}

static void __exit SZHC_Buttons_exit (void)
{
    int row,column;

    //free timers
	del_timer (&szhc_buttons_dev->s_timer);
#ifdef ADD_BUZZER_WHIT_BUTTON
	del_timer (&szhc_buttons_dev->b_timer);
#endif

    //free gpio
    gpio_free(GPIO_TO_PIN(1,19);
	for (row = 0; row < RowCount; row++)
	{
		gpio_free (rowtable[row]);
	}
	for (column = 0; column < ColumnCount; column++)
	{
	 	gpio_free (columtable[column]);
	}
	kfree (szhc_buttons_dev);
	unregister_chrdev (SZHC_BUTTON_MAJOR, DEVICE_NAME);
	device_destroy (szhc_buttons_class, MKDEV (SZHC_BUTTON_MAJOR, 0));
	class_destroy (szhc_buttons_class);
}

module_init (SZHC_Buttons_init);
module_exit (SZHC_Buttons_exit);

MODULE_AUTHOR ("Fanyukui in SZHC");
MODULE_DESCRIPTION ("AM335x Buttons Driver");
MODULE_LICENSE ("GPL");
