/*************************************
NAME:szhc_keypad_jixieshou.c
COPYRIGHT:SZHC
*************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/irq.h>
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
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

/* 加载模式后,执行”cat /proc/devices”命令看到的设备名称 */
#define DEVICE_NAME "szhc_keypads"


#define ColumnCount   6
#define RowCount   5

char keypad[RowCount][ColumnCount];
unsigned int columtable[ColumnCount];
unsigned int rowtable[RowCount];

unsigned int knob[3] = {80,81,82};
unsigned int pulley[3] = {90,91,92};
static int rowNum,colomnNum;

static int pressed;


#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

static struct timer_list s_timer;
static struct input_dev  *button_dev; /*输入设备结构体*/

#define Beep_On()   do {gpio_set_value(GPIO_TO_PIN(1,19), 1); } while(0)
#define Beep_Off()  do {gpio_set_value(GPIO_TO_PIN(1,19), 0); } while(0)

void initKeyPad()
{
    int start = 1,i,j;
    for(i=0;i<RowCount;i++)
    {
        for(j=0;j<ColumnCount;j++)
        {
            keypad[i][j] = start++;
        }

    }

}

void initPinOut(unsigned int gpio,bool isOut,char *label)
{
    int result;
    /* Allocating GPIOs and setting direction */
    result = gpio_request(gpio, label);//usr1
    if (result != 0)
        printk("gpio_request(%d) failed!\n",gpio);
    if(isOut){
        result = gpio_direction_output(gpio,0);  //beep 拉低
        if (result != 0)
            printk("gpio_direction(%d) failed!\n",gpio);

    }
    else{
        result = gpio_direction_input(gpio);
        if (result != 0)
            printk("gpio_direction(%d) failed!\n",gpio);
    }

}

irqreturn_t irq_handler(int irqno, void *dev_id)

{
 //   printk("GPIO_TO_PIN(2,4) :%d\n",gpio_get_value(GPIO_TO_PIN(2,4)));

    if(gpio_get_value(GPIO_TO_PIN(2,4)))
    {
  			input_report_key(button_dev,pulley[1], true);
  			input_report_key(button_dev,pulley[1], false);

    }
    else{
  			input_report_key(button_dev,pulley[2], true);
  			input_report_key(button_dev,pulley[2], false);


    }
    return IRQ_HANDLED;
}


void initConfig(void)
{
    int row,column;
    int irq = 0,ret = 0;

    /*蜂鸣器 输出*/
  	initPinOut(GPIO_TO_PIN(1,19),true,"Beep");

    /*旋钮 输入*/
	initPinOut(GPIO_TO_PIN(0,13),false,"Knob"); //停止
	initPinOut(GPIO_TO_PIN(2,5),false,"Knob");  //自动 低电平有效
	initPinOut(GPIO_TO_PIN(2,2),false,"Knob");  //手动

    /*滚轮 输入*/
    initPinOut(GPIO_TO_PIN(2,1),false,"pulley");
    initPinOut(GPIO_TO_PIN(2,3),false,"pulley");  /*中断口*/
    initPinOut(GPIO_TO_PIN(2,4),false,"pulley");

    gpio_set_debounce(GPIO_TO_PIN(2,3),25);

    /*申请中断*/
    irq = gpio_to_irq(GPIO_TO_PIN(2, 3));
    if(irq < 0){
        printk("GPIO_TO_PIN(2, 3) irq  failed !\n");
        ret = irq;
    }
    //printk("irq = %d\n", irq);
    ret = request_irq(irq,
                    irq_handler,
                    IRQF_TRIGGER_FALLING | IRQF_SHARED,
                    "pulley",
                    irq_handler);
    if(ret){
        printk("request_irq() failed ! %d\n", ret);
    }
    /*初始化 rowtable*/
    rowtable[0] = GPIO_TO_PIN(3,14);
    rowtable[1] = GPIO_TO_PIN(3,15);
    rowtable[2] = GPIO_TO_PIN(3,16);
    rowtable[3] = GPIO_TO_PIN(3,17);
    rowtable[4] = GPIO_TO_PIN(3,18);
  //  rowtable[5] = GPIO_TO_PIN(3,19);
   // rowtable[6] = GPIO_TO_PIN(3,20);
   // rowtable[7] = GPIO_TO_PIN(3,21);
    /*行 输入*/
    for(row=0;row<RowCount;row++)
    {
        initPinOut(rowtable[row],false,"KeyPadInput");
    }

    /*初始化 columntable*/
    columtable[0] = GPIO_TO_PIN(1,27);
    columtable[1] = GPIO_TO_PIN(1,26);
    columtable[2] = GPIO_TO_PIN(1,25);
    columtable[3] = GPIO_TO_PIN(0,31);
    columtable[4] = GPIO_TO_PIN(1,17);
    columtable[5] = GPIO_TO_PIN(3,19);
  //  columtable[5] = GPIO_TO_PIN(1,16);
  //  columtable[6] = GPIO_TO_PIN(0,30);
  //  columtable[7] = GPIO_TO_PIN(1,28);

    /*列 输出*/
    for(column=0;column<ColumnCount;column++)
    {
        initPinOut(columtable[column],true,"KeyPadOutput");
    }

}


/*拉低指定列*/
void downColumn(int index)
{
   int i;
   for(i=0;i<ColumnCount;i++)
   {
        if(i==index)
            gpio_set_value(columtable[i],0);
        else
            gpio_set_value(columtable[i],1);
   }

}


void timer_handler(unsigned long arg)
{
    int col = 0,row = 0;

    /*处理旋钮*/
  	int p1 = gpio_get_value(GPIO_TO_PIN(0,13));
    int p2 = gpio_get_value(GPIO_TO_PIN(2,5));
    int p3 = gpio_get_value(GPIO_TO_PIN(2,2));
    int value = (p1 << 2) | (p2 << 1) | p3;
    //printk("value: %d\n",value);
    if(value == 6){
  			input_report_key(button_dev,knob[0], true);
   			input_report_key(button_dev,knob[1], false);
   			input_report_key(button_dev,knob[2], false);
            input_sync(button_dev);
    }
    else if(value == 3){
  			input_report_key(button_dev,knob[1], true);
  			input_report_key(button_dev,knob[2], false);
   			input_report_key(button_dev,knob[0], false);

            input_sync(button_dev);

    }
    else if(value == 1){
  			input_report_key(button_dev,knob[2], true);
  			input_report_key(button_dev,knob[0], false);
   			input_report_key(button_dev,knob[1], false);
            input_sync(button_dev);

    }

    /*处理滚轮按下事件*/
    if(gpio_get_value(GPIO_TO_PIN(2,1)) == 0)
    {
  			input_report_key(button_dev,pulley[0], true);
            input_sync(button_dev);

    }
    else{
  			input_report_key(button_dev,pulley[0], false);
            input_sync(button_dev);
    }

    /*处理普通按键*/
    for(col=0;col<ColumnCount;col++)
    {
        downColumn(col);
        for(row=0;row<RowCount;row++)
        {
            if(gpio_get_value(rowtable[row]) == 0 && !pressed)
            {
                pressed = 1;
                Beep_On();
      			input_report_key(button_dev,keypad[row][col], 1);
                rowNum = row;
                colomnNum = col;
                goto finished;
                //printk("row: %d\tcolumn:%d\tkey value: %d \t pressed!\n",row,col,keypad[row][col]);
                //input_sync(button_dev);
            }
            if(col == colomnNum && gpio_get_value(rowtable[rowNum]))
            {
                //printk("row: %d\tcolumn:%d\tkey value: %d \t released!\n",row,col,keypad[row][col]);
                pressed = 0;
                Beep_Off();
      			input_report_key(button_dev,keypad[row][col], 0);
            }
        }

    }

finished:
    input_sync(button_dev);
  	unsigned long j = jiffies;
	s_timer.expires = j + HZ/50 ;
	add_timer(&s_timer);
}


void initTimer(void)
{
   unsigned long j = jiffies;
   init_timer(&s_timer);
   s_timer.function = timer_handler;
   s_timer.expires = j + HZ;

   add_timer(&s_timer);

}



static char __initdata banner[] = "SZHCAM335x  Keypad, (c) 2014 SZHC\n";
//static struct class *keypad_class;

static int __init szhc_keypad_init(void)
{
    int error;
    printk(banner);

    /*初始化KeyPad配置*/
    initConfig();

    /*初始化KeyPad值*/
    initKeyPad();

    button_dev = input_allocate_device(); /*分配一个设备结构体*/
    if(!button_dev)
    {
        printk("Not enough memory\n");
        error = -ENOMEM;
        goto error_free_dev;
    }

    __set_bit(EV_KEY,button_dev->evbit);//支持EV_KEY事件
    //__set_bit(EV_REP,button_dev->evbit); //支持EV_REP事件

    //button_dev->evbit[0] = BIT_MASK(EV_KEY);  //支持EV_KEY事件
    //button_dev->evbit[0] = BIT_MASK(EV_REP);  //支持EV_REP事件

    int i,j;
    for(i=0; i<RowCount;i++)
    {
        for(j=0;j<ColumnCount;j++)
        {
            //支持普通键
	    	__set_bit(keypad[i][j], button_dev->keybit);
        }
    }
    /*支持旋钮*/
    for(i=0; i<3;i++)
        __set_bit(knob[i], button_dev->keybit);

    /*支持滑轮*/
    for(i=0;i<3;i++)
        __set_bit(pulley[i],button_dev->keybit);


  	error = input_register_device(button_dev);

    if(error){
        printk("failed to register device\n");
        goto error_free_dev;

    }

    /*初始化Timer配置*/
    initTimer();

    return 0;

    error_free_dev:
        input_free_device(button_dev);

    return error;

}

static void __exit szhc_keypad_exit(void)
{
    /* 卸载驱动程序 */
  	del_timer(&s_timer);
    input_unregister_device(button_dev);
    /* free gpio */
    int irq = gpio_to_irq(GPIO_TO_PIN(2, 3));
    free_irq(irq,irq_handler) ;
    //gpio_free(GPIO_TO_PIN(1, 27));
}

/* 这两行指定驱动程序的初始化函数和卸载函数 */
module_init(szhc_keypad_init);
module_exit(szhc_keypad_exit);

/* 描述驱动程序的一些信息,不是必须的 */
MODULE_AUTHOR("FANYUKUI in SZHC");// 驱动程序的作者
MODULE_DESCRIPTION("AM335x LED Driver"); // 一些描述信息
MODULE_LICENSE("GPL");  // 遵循的协议
