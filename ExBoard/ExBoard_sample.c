#include "pinctrl.h"
#include "soc_osal.h"
#include "gpio.h"
#include "dma.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "app_init.h"
#include "pwm_ex.h"
#include "los_task.h"
#include "watchdog.h"
#include "common_def.h"
#include "math.h"
//private
#include "my_iic.h"
#include "aht20.h"
#include "bh1750.h"
#include "ws2812.h"
#include "sle_uart_server.h"
#include "motor.h"

#define ExBoard_TASK_STACK_SIZE 0x20000
#define ExBoard_TASK_PRIO (osPriority_t)(17)

// WatchDog相关参数
#define TIME_OUT 2
#define WDT_MODE 1

#define LED_COUNT 12

uint8_t sg90_ctrl = 0;

uint16_t sg90_angles[4] = {0};
uint8_t *message_from_ai[30] = {0};
float temperature, humidity;
uint16_t light;
uint8_t weather;

static errcode_t watchdog_callback(uintptr_t param)
{
    UNUSED(param);
    osal_printk("watchdog kick timeout!\r\n");
    return ERRCODE_SUCC;
}

// 发送数据后的回调函数
static void sle_server_read_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para,
                                          errcode_t status)
{
    osal_printk("%s ssaps read request cbk callback server_id:%x, conn_id:%x, handle:%x, status:%x\r\n",
                SLE_UART_SERVER_LOG, server_id, conn_id, read_cb_para->handle, status);
}

// 接收数据后的回调函数
static void sle_server_write_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para,
                                           errcode_t status)
{
    osal_printk("%s Write request received, server_id: %d, conn_id: %d, status: 0x%x\r\n",
                SLE_UART_SERVER_LOG, server_id, conn_id, status);

    if (write_cb_para != NULL)
    {

        if (write_cb_para->value[0] == 'E')
        {
            osal_printk("receive:%s\r\n", write_cb_para->value);
        }
        else if (write_cb_para->value[0] == 'A')
        {
            sscanf((char *)write_cb_para->value, "A s:%d e:%d e:%d e:%d w:%d m:%s", sg90_angles, sg90_angles + 1, sg90_angles + 2, sg90_angles + 3, &weather, message_from_ai);
            osal_printk("%s Received data (len=%d): ", SLE_UART_SERVER_LOG, write_cb_para->length);
            osal_printk("%s", write_cb_para->value);
            osal_printk("\r\n");
        }

        uint8_t response[] = {0x01, 0x02, 0x03}; // Example response
        sle_uart_server_send_report_by_handle(response, sizeof(response));
    }
}

void *sg90_start(const char *arg)
{
    uint8_t gpio = *(uint8_t *)arg;
    while (sg90_ctrl == 1)
    {
        uint32_t high = (uint32_t)(500 + sg90_angles[gpio] * 11.11 + 0.5);
        uint32_t low = 20000 - high;
        uapi_gpio_set_val(gpio, GPIO_LEVEL_HIGH);
        osal_udelay(high); // 会阻塞CPU
        uapi_gpio_set_val(gpio, GPIO_LEVEL_LOW);
        osal_msleep(low/1000-10);//较长的延迟（大于10ms）使用msleep，防止线程阻塞，可能有10ms左右的额外开销
    }
    return NULL;
}

void *BH_AHT_start(const char *arg)
{
    unused(arg);
    while (1)
    {
        // 读取AHT20数据
        AHT20_ReadData(&temperature, &humidity);

        // 读取BH1750数据
        light = BH1750_ReadLight();

        char buffer[30] = {0};
        snprintf(buffer, sizeof(buffer), "E t:%.2f h:%.2f l:%d", temperature, humidity, light);
        osal_printk("%s\r\n", buffer); // 不支持直接输出浮点数

        // 星闪发送数据
        if (sle_uart_client_is_connected())
        {
            sle_uart_server_send_report_by_handle((uint8_t *)buffer, sizeof(buffer));
        }

        if (sg90_angles[0]){
            // 启动电机
            motor_forward();
        }
        else{
            motor_stop();
        }
        (void)uapi_watchdog_kick(); // 必须进行喂狗，不然会重启
        osal_msleep(1000);
    }
    return NULL;
}

static void *ExBoard_task(const char *arg)
{
    unused(arg);
    osKernelStart();
    uint8_t sle_connect_state[] = "sle_dis_connect";
    uint8_t buffer[SLE_UART_SERVER_MSG_QUEUE_MAX_SIZE] = {0};
    uint16_t length = SLE_UART_SERVER_MSG_QUEUE_MAX_SIZE;
    errcode_t ret;

    // 初始化看门狗
    ret = uapi_watchdog_init(TIME_OUT);
    if (ret == ERRCODE_INVALID_PARAM)
    {
        osal_printk("param is error, timeout is %d.\r\n", TIME_OUT);
        return NULL;
    }
    (void)uapi_watchdog_enable((wdt_mode_t)WDT_MODE);
    (void)uapi_register_watchdog_callback(watchdog_callback);
    osal_printk("init watchdog\r\n");

    // 初始化各种驱动
    uapi_dma_init();
    uapi_dma_open();
    PWM_init();
    motor_gpio_init();
    AHT20_Init();
    BH1750_Init();
    iic_init();
    spi_init();

    // 初始化星闪
    sle_uart_server_init(sle_server_read_cbk, sle_server_write_cbk);

    // 初始化sg90任务参数
    sg90_ctrl = 1;
    sg90_angles[0] = 1;
    sg90_angles[1] = 60;
    sg90_angles[2] = 60;
    sg90_angles[3] = 60;

    // 挂载sg90线程
    osThreadAttr_t sg90_task = {
        .name = "SG90Task_1",
        .attr_bits = 0U,
        .cb_mem = NULL,
        .cb_size = 0U,
        .stack_mem = NULL,
        .stack_size = 0x2000,
        .priority = 15};
    uint8_t gpios[3] = {PWM_GPIO_1, PWM_GPIO_2, PWM_GPIO_3};
    for (int i = 0; i < 3; i++)
    {
        osThreadNew((osThreadFunc_t)sg90_start, &gpios[i], &sg90_task);
        if (i)
        {
            sg90_task.priority=14;
            sg90_task.name = "SG90Task_3";
        }
        else
        {
            sg90_task.priority=13;
            sg90_task.name = "SG90Task_2";
        }
        osal_printk("SG90Task create succ\r\n");
    }

    // 挂载传感器线程
    osThreadAttr_t BH_AHT_task = {
        .name = "BH_AHT_task",
        .attr_bits = 0U,
        .cb_mem = NULL,
        .cb_size = 0U,
        .stack_mem = NULL,
        .stack_size = 0x2000,
        .priority = 16};
    if (osThreadNew((osThreadFunc_t)BH_AHT_start, NULL, &BH_AHT_task) == NULL)
    {
        osal_printk("BH_AHT_task create fail\r\n");
    }
    else
    {
        osal_printk("BH_AHT_task create succ\r\n");
    }

    float hue = 0;
    uint8_t r[LED_COUNT], g[LED_COUNT], b[LED_COUNT];
    while (1)
    {
        for (int i=0;i<LED_COUNT;i++){
            float led_hue=fmod(hue+i*20.0,360.0);
            hsv2rgb(hue, 1.0, 0.3, r+i, g+i, b+i);
        }
        for (int i=0;i<LED_COUNT;i++){
            ws2812_send_rgb(r[i], g[i], b[i]);
        }
        ws2812_reset();
        hue = fmod(hue + 1, 360.0); // 色相变化
        osal_msleep(1);
    }

    sle_uart_server_delete_msgqueue();
    return NULL;
}

static void ExBoard_entry(void)
{
    osThreadAttr_t attr;

    attr.name = "ExBoardTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = ExBoard_TASK_STACK_SIZE;
    attr.priority = ExBoard_TASK_PRIO;

    if (osThreadNew((osThreadFunc_t)ExBoard_task, NULL, &attr) == NULL)
    {
        /* Create task fail. */
    }
}

/* Run the ExBoard_entry. */
app_run(ExBoard_entry);