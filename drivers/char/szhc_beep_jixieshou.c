/********************************************************************
**
*
*
*
*
*
*
*
********************************************************************/

#include <linux/module.h>	//$(TOPDIR)/include/linux/...
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <asm/io.h>

#define DEVICE_NAME		"szhc_beep"
#define BUZZER_MAJOR	231

#define IOCTL_BUZZER_ON		1
#define IOCTL_BUZZER_OFF	0

#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

static bool isBeep;

static int buzzer_open(struct inode *inode,struct file *file){
	printk("**buzzer_open(drivers/char/buzzer.c)**\n");
	return 0;
}


static long buzzer_ioctl(struct file *file,unsigned int cmd,unsigned long arg){

	switch(arg){
		case IOCTL_BUZZER_OFF:
            if(isBeep)
            {
                isBeep = false;
       			gpio_free(GPIO_TO_PIN(1,19));
      			printk("**BUZZER_OFF(drivers/char/buzzer.c)**\n");
            }
			return 0;

		case IOCTL_BUZZER_ON:
            if(!isBeep)
            {
                isBeep = true;
    			gpio_request(GPIO_TO_PIN(1,19),"Beep");
                gpio_direction_output(GPIO_TO_PIN(1,19),0);  //beep À­µÍ
    			printk("**BUZZER_ON(drivers/char/buzzer.c)**\n");
            }
            return 0;
		default:
			return -EINVAL;
	}
}


static struct file_operations buzzer_fops={
	.owner	=	THIS_MODULE,
	.open = buzzer_open,
	.unlocked_ioctl = buzzer_ioctl,
};

static char __initdata banner[] = "SZHC 2014 Beep Driver\n";
static struct class *buzzer_class;

static int __init buzzer_init(void){
	int ret;
    isBeep = true;
	printk(banner);
	ret = register_chrdev(BUZZER_MAJOR,DEVICE_NAME,&buzzer_fops);
	if(ret<0){
		printk("\n"DEVICE_NAME" can't register major number\n");
		return ret;
	}

	buzzer_class = class_create(THIS_MODULE,DEVICE_NAME);
	if(IS_ERR(buzzer_class)){
		printk("Err:failed in buzzer class.\n");
		return -1;
	}

	device_create(buzzer_class,NULL,MKDEV(BUZZER_MAJOR,0),NULL,DEVICE_NAME);

	return 0;
}

static void __exit buzzer_exit(void){
	printk("\n**buzzer_exit(drivers/char/buzzer.c)**\n");
	unregister_chrdev(BUZZER_MAJOR,DEVICE_NAME);
	device_destroy(buzzer_class,MKDEV(BUZZER_MAJOR,0));
	class_destroy(buzzer_class);
}

module_init(buzzer_init);
module_exit(buzzer_exit);
MODULE_AUTHOR("Fanyukui in SZHC");
MODULE_DESCRIPTION("AM335x Beep Driver");
MODULE_LICENSE("GPL");


