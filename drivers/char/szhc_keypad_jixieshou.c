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
#include <asm/io.h>

/* ����ģʽ��,ִ�С�cat /proc/devices����������豸���� */
#define DEVICE_NAME "szhc_keypads"


#define ColumnCount   8
#define RowCount   8

char keypad[RowCount][ColumnCount];
unsigned int columtable[ColumnCount];
unsigned int rowtable[RowCount];

unsigned int knob[3] = {80,81,82};
unsigned int pulley[3] = {90,91,92};
static int rowNum,colomnNum;
static int pressed;

#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))


#define MODE7 7

/*I/O״̬����*/
#define IEN (1 << 5)
#define PD (0 << 3)
#define PU (2 << 3)
#define OFF (1 << 3)
#define GPIO_MOD_CTRL_BIT	BIT(0)

/*Module Control �Ĵ���*/
#define AM335X_CTRL_BASE			0x44E10000

#define CONTROL_PADCONF_GPMC_CSN0                 0x087C
#define CONTROL_PADCONF_MCASP0_AHCLKX             0x09AC
#define CONTROL_PADCONF_EMU0                      0x09E4
#define CONTROL_PADCONF_EMU1                      0x09E8
#define CONTROL_PADCONF_MCASP0_ACLKR              0x09A0
#define CONTROL_PADCONF_XDMA_EVENT_INTR0          0x09B0
#define CONTROL_PADCONF_USB0_DRVVBUS              0x0A1C
#define CONTROL_PADCONF_USB1_DRVVBUS              0x0A34


/*ģʽ����*/
#define MUX_VAL(OFFSET,VALUE)\
    writel((VALUE) | (readl(p+(OFFSET)) & ~0x7), p + (OFFSET));

#define MUX_READ(OFFSET)    \
    readl(p+(OFFSET))

/*ģʽ����*/
#define MUX_EVM() \
    MUX_VAL(CONTROL_PADCONF_GPMC_CSN0, (IEN | PU | MODE7 )) /* gpio2[1] */\
    MUX_VAL(CONTROL_PADCONF_MCASP0_AHCLKX, (IEN | PU | MODE7 )) /* timer7_mux3(GPIO2_3) */\
    MUX_VAL(CONTROL_PADCONF_EMU0, (IEN | PU | MODE7 )) /* timer7_mux3(GPIO2_3) */\
    MUX_VAL(CONTROL_PADCONF_EMU1, (IEN | PU | MODE7 )) /* timer7_mux3(GPIO2_3) */\
    MUX_VAL(CONTROL_PADCONF_MCASP0_ACLKR, (IEN | PU | MODE7 )) /* timer7_mux3(GPIO2_3) */\
    MUX_VAL(CONTROL_PADCONF_XDMA_EVENT_INTR0, (IEN | PU | MODE7 )) /* timer7_mux3(GPIO2_3) */  \
    MUX_VAL(CONTROL_PADCONF_USB0_DRVVBUS, (IEN | PU | MODE7 )) /* timer7_mux3(GPIO2_3) */   \
    MUX_VAL(CONTROL_PADCONF_USB1_DRVVBUS, (IEN | PU | MODE7 )) /* timer7_mux3(GPIO2_3) */


static struct timer_list s_timer;
static struct input_dev  *button_dev; /*�����豸�ṹ��*/

#define Beep_On()   do {gpio_set_value(GPIO_TO_PIN(1,19), 1); } while(0)
#define Beep_Off()  do {gpio_set_value(GPIO_TO_PIN(1,19), 0); } while(0)

void initKeyPad(void)
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

void initPinOut(unsigned int gpio,bool isOut,char *label,unsigned int high)
{
    int result;
    /* Allocating GPIOs and setting direction */
    result = gpio_request(gpio, label);//usr1
    if (result != 0)
        printk("gpio_request(%d) failed!\n",gpio);
    if(isOut){
        result = gpio_direction_output(gpio,high);  //beep ����
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


    /*����ģʽ*/
    void __iomem *p = ioremap_nocache(AM335X_CTRL_BASE ,SZ_4K);
    if(!p){
        printk("ioremap failed\n");
        return;
    }
    MUX_EVM();
    /*printk("CONTROL_PADCONF_GPMC_CSN0: 0x%x\n",MUX_READ(CONTROL_PADCONF_GPMC_CSN0));
    printk("CONTROL_PADCONF_MCASP0_AHCLKX: 0x%x\n",MUX_READ(CONTROL_PADCONF_MCASP0_AHCLKX));
    printk("CONTROL_PADCONF_EMU0: 0x%x\n",MUX_READ(CONTROL_PADCONF_EMU0));
    printk("CONTROL_PADCONF_EMU1: 0x%x\n",MUX_READ(CONTROL_PADCONF_EMU1));
    printk("CONTROL_PADCONF_MCASP0_ACLKR: 0x%x\n",MUX_READ(CONTROL_PADCONF_MCASP0_ACLKR));
    printk("CONTROL_PADCONF_XDMA_EVENT_INTR0: 0x%x\n",MUX_READ(CONTROL_PADCONF_XDMA_EVENT_INTR0));
    */
    iounmap(p);

    /*������ ���*/
  	initPinOut(GPIO_TO_PIN(1,19),true,"Beep",0);

    /*��ť ����*/
	initPinOut(GPIO_TO_PIN(2,0),false,"Knob",0); //ֹͣ
	initPinOut(GPIO_TO_PIN(2,5),false,"Knob",0);  //�Զ� �͵�ƽ��Ч
	initPinOut(GPIO_TO_PIN(2,2),false,"Knob",0);  //�ֶ�

    /*���� ����*/
    initPinOut(GPIO_TO_PIN(2,1),false,"pulley",0);
    initPinOut(GPIO_TO_PIN(2,3),false,"pulley",0);  /*�жϿ�*/
    initPinOut(GPIO_TO_PIN(2,4),false,"pulley",0);

    gpio_set_debounce(GPIO_TO_PIN(2,3),25);

    /*�����ж�*/
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
    /*��ʼ�� rowtable*/
    rowtable[0] = GPIO_TO_PIN(3,8);
    rowtable[1] = GPIO_TO_PIN(3,7);
    rowtable[2] = GPIO_TO_PIN(1,29);
    rowtable[3] = GPIO_TO_PIN(3,21);
    rowtable[4] = GPIO_TO_PIN(3,18);
    rowtable[5] = GPIO_TO_PIN(0,19);
    rowtable[6] = GPIO_TO_PIN(0,18);
    rowtable[7] = GPIO_TO_PIN(3,13);
    /*�� ����*/
    for(row=0;row<RowCount;row++)
    {
        initPinOut(rowtable[row],false,"KeyPadInput",0);
    }

    /*��ʼ�� columntable*/
    columtable[0] = GPIO_TO_PIN(1,27);
    columtable[1] = GPIO_TO_PIN(1,26);
    columtable[2] = GPIO_TO_PIN(1,25);
    columtable[3] = GPIO_TO_PIN(0,31);
    columtable[4] = GPIO_TO_PIN(1,17);
    columtable[5] = GPIO_TO_PIN(1,16);
    columtable[6] = GPIO_TO_PIN(0,30);
    columtable[7] = GPIO_TO_PIN(1,28);

    /*�� ���*/
    for(column=0;column<ColumnCount;column++)
    {
        initPinOut(columtable[column],true,"KeyPadOutput",1);
    }

}


/*����ָ����*/
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

    /*������ť*/
  	int p1 = gpio_get_value(GPIO_TO_PIN(2,0));
    int p2 = gpio_get_value(GPIO_TO_PIN(2,5));
    int p3 = gpio_get_value(GPIO_TO_PIN(2,2));
    int value = (p1 << 2) | (p2 << 1) | p3;
    //printk("value: %d\n",value);
    if(value == 6){  //�ֶ�
  			input_report_key(button_dev,knob[0], true);
   			input_report_key(button_dev,knob[1], false);
   			input_report_key(button_dev,knob[2], false);
            input_sync(button_dev);
    }
    else if(value == 5){  //ֹͣ
  			input_report_key(button_dev,knob[1], true);
  			input_report_key(button_dev,knob[2], false);
   			input_report_key(button_dev,knob[0], false);

            input_sync(button_dev);

    }
    else if(value == 1){ //�Զ�
  			input_report_key(button_dev,knob[2], true);
  			input_report_key(button_dev,knob[0], false);
   			input_report_key(button_dev,knob[1], false);
            input_sync(button_dev);

    }

    /*������ְ����¼�*/
    if(gpio_get_value(GPIO_TO_PIN(2,1)) == 0)
    {
  			input_report_key(button_dev,pulley[0], true);
            input_sync(button_dev);

    }
    else{
  			input_report_key(button_dev,pulley[0], false);
            input_sync(button_dev);
    }

    /*������ͨ����*/
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
                //printk("row: %d\tcolumn:%d\tkey value: %d \t pressed!\n",row,col,keypad[row][col]);
                goto finished;
                //input_sync(button_dev);
            }
            if(col == colomnNum && row == rowNum &&
                gpio_get_value(rowtable[rowNum]) && pressed)
            {
               // printk("row: %d\tcolumn:%d\tkey value: %d \t released!\n",row,col,keypad[row][col]);
                pressed = 0;
                Beep_Off();
      			input_report_key(button_dev,keypad[row][col], 0);
            }
        }

    }

finished:
    input_sync(button_dev);
	s_timer.expires = jiffies + HZ/50 ;
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
    int error,i,j;
    printk(banner);

    /*��ʼ��KeyPad����*/
    initConfig();

    /*��ʼ��KeyPadֵ*/
    initKeyPad();

    button_dev = input_allocate_device(); /*����һ���豸�ṹ��*/
    if(!button_dev)
    {
        printk("Not enough memory\n");
        error = -ENOMEM;
        goto error_free_dev;
    }

    __set_bit(EV_KEY,button_dev->evbit);//֧��EV_KEY�¼�
    //__set_bit(EV_REP,button_dev->evbit); //֧��EV_REP�¼�

    //button_dev->evbit[0] = BIT_MASK(EV_KEY);  //֧��EV_KEY�¼�
    //button_dev->evbit[0] = BIT_MASK(EV_REP);  //֧��EV_REP�¼�

    for(i=0; i<RowCount;i++)
    {
        for(j=0;j<ColumnCount;j++)
        {
            //֧����ͨ��
	    	__set_bit(keypad[i][j], button_dev->keybit);
        }
    }
    /*֧����ť*/
    for(i=0; i<3;i++)
        __set_bit(knob[i], button_dev->keybit);

    /*֧�ֻ���*/
    for(i=0;i<3;i++)
        __set_bit(pulley[i],button_dev->keybit);


  	error = input_register_device(button_dev);

    if(error){
        printk("failed to register device\n");
        goto error_free_dev;

    }

    /*��ʼ��Timer����*/
    initTimer();

    return 0;

    error_free_dev:
        input_free_device(button_dev);

    return error;

}

static void __exit szhc_keypad_exit(void)
{
    int irq ;
    /* ж���������� */
  	del_timer(&s_timer);
    input_unregister_device(button_dev);
    /* free gpio */
    irq = gpio_to_irq(GPIO_TO_PIN(2, 3));
    free_irq(irq,irq_handler) ;
    //gpio_free(GPIO_TO_PIN(1, 27));
}

/* ������ָ����������ĳ�ʼ��������ж�غ��� */
module_init(szhc_keypad_init);
module_exit(szhc_keypad_exit);

/* �������������һЩ��Ϣ,���Ǳ���� */
MODULE_AUTHOR("FANYUKUI in SZHC");// �������������
MODULE_DESCRIPTION("AM335x LED Driver"); // һЩ������Ϣ
MODULE_LICENSE("GPL");  // ��ѭ��Э��
