/******************************************************************************

 ******************************************************************************
  文 件 名   : szhc_pll.c
  版 本 号   : 初稿
  作    者   : fanyukui
  生成日期   : 2015年8月7日
  最近修改   :
  功能描述   :
  函数列表   :
              InitPLL
              InitTimer
              pll_exit
              pll_init
              pll_ioctl
              pll_open
              timer_handler
  修改历史   :
  1.日    期   : 2015年8月7日
    作    者   : fanyukui
    修改内容   : 创建文件

******************************************************************************/

#include <linux/module.h>		//$(TOPDIR)/include/linux/...
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <linux/clk-provider.h>
#include <linux/uaccess.h>


#define DEVICE_NAME		"szhc_pll"
#define PLL_MAJOR	233

enum PLL_IOCTL
{
	SET_PER_PLL = 100,
	SET_DIS_PLL,
	SET_TIMER,
	GET_PER_PLL,
	GET_DIS_PLL,
	GET_TIMER
};

#define DEFAULT_INTERVAL 60		//60s

typedef struct _pll_dev
{
	struct clk *per_clk;			//外设PLL
	struct clk *dis_clk;			//显示PLL
	long interval;				//定时器周期
	long per_rate;				//外设时钟
	long dis_rate;				//显示时钟
} Pll_Dev;

static Pll_Dev pll_dev;
static struct timer_list s_timer;

static long pll_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
    printk("cmd: %d  arg: %lu\n",cmd,arg);
	switch (cmd)
	{
	case SET_PER_PLL:
		pll_dev.per_rate = arg;
		return 0;
	case SET_DIS_PLL:
		pll_dev.dis_rate = arg;
		return 0;
	case SET_TIMER:
		pll_dev.interval = arg;
		return 0;
	case GET_PER_PLL:
		return 0;
	case GET_DIS_PLL:
		return 0;
	case GET_TIMER:
		return 0;
	default:
		return -EINVAL;
	}
}

void timer_handler (unsigned long arg)
{
    int ret;
    /* printk("timer_handler.....\n"); */
	ret = clk_set_rate(pll_dev.per_clk, pll_dev.per_rate);
	if (ret < 0) {
		pr_warning("failed to set peripheral clock rate to %lu: %d\n",
			pll_dev.per_rate, ret);
		return;
	}


	ret = clk_set_rate(pll_dev.dis_clk, pll_dev.dis_rate);
	if (ret < 0) {
		pr_warning("failed to set peripheral clock rate to %lu: %d\n",
			pll_dev.dis_rate, ret);
		return;
	}

  	s_timer.expires = jiffies + HZ * pll_dev.interval;
    add_timer(&s_timer);

}

int InitPLL (void)
{
	pll_dev.interval = DEFAULT_INTERVAL;
	pll_dev.per_clk = clk_get_sys (NULL, "dpll_per_ck");
	pll_dev.dis_clk = clk_get_sys (NULL, "dpll_disp_ck");
    pll_dev.per_rate = clk_get_rate(pll_dev.per_clk);
	pll_dev.dis_rate = clk_get_rate(pll_dev.dis_clk);
	if (!pll_dev.per_clk || !pll_dev.dis_clk)
	{
		return -1;
	}
	else
	{
	    printk("Per PLL:%lu\n", pll_dev.per_rate);
	    printk("Dis PLL:%lu\n", pll_dev.dis_rate);
		return 0;
	}
}

void InitTimer (void)
{
	unsigned long j = jiffies;
	init_timer (&s_timer);
	s_timer.function = timer_handler;
	s_timer.expires = j + HZ * pll_dev.interval;
	add_timer (&s_timer);
}

static int pll_open (struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations pll_fops = {
	.owner = THIS_MODULE,.open = pll_open,.unlocked_ioctl = pll_ioctl
};

static char __initdata banner[] = "SZHC 2014 PLL Driver\n";
static struct class *pll_class;
static int __init pll_init (void)
{
	int ret;
	printk (banner);
	ret = register_chrdev (PLL_MAJOR, DEVICE_NAME, &pll_fops);
	if (ret < 0)
	{
		printk ("\n" DEVICE_NAME " can't register major number\n");
		return ret;
	}

	pll_class = class_create (THIS_MODULE, DEVICE_NAME);
	if (IS_ERR (pll_class))
	{
		printk ("Err:failed in buzzer class.\n");
		return -1;
	}

	device_create (pll_class, NULL, MKDEV (PLL_MAJOR, 0), NULL, DEVICE_NAME);
	/*初始化PLL配置 */
	ret = InitPLL ();
	if (ret)
	{
		printk ("Get Pll Failed\n");
		goto failed;
	}

	/*初始化Timer配置 */
	InitTimer ();
	return 0;
  failed:
	unregister_chrdev (PLL_MAJOR, DEVICE_NAME);
	device_destroy (pll_class, MKDEV (PLL_MAJOR, 0));
	class_destroy (pll_class);
	return -1;
}

static void __exit pll_exit (void)
{
	unregister_chrdev (PLL_MAJOR, DEVICE_NAME);
	device_destroy (pll_class, MKDEV (PLL_MAJOR, 0));
	class_destroy (pll_class);
}

module_init (pll_init);
module_exit (pll_exit);
MODULE_AUTHOR ("Fanyukui in SZHC");
MODULE_DESCRIPTION ("AM335x PLL Driver");
MODULE_LICENSE ("GPL");
