// 中控面板电子纸主程序
// 同时承担sle星闪client接收数据

#include "pinctrl.h"
#include "soc_osal.h"
#include "gpio.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "app_init.h"
#include "los_sys.h"
#include "string.h"
#include "stdlib.h"
#include "sle_low_latency.h"
#include "common_def.h"
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"
// private
#include "sle_uart_client.h"
#include "EPD.h"
#include "EPD_GUI.h"
#include "pic.h"

#define EPD_TASK_STACK_SIZE 0x2000
#define EPD_TASK_PRIO (osPriority_t)(17)
float temperature;
float humidity;
uint16_t light;
char transmit_message[30] = {0};

void sle_uart_notification_cb(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
                              errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);
    if (data->data[0] == 'E')
    {
        sscanf((char *)data->data, "E t:%f h:%f l:%d", &temperature, &humidity, &light);
        sprintf(transmit_message, "E %.2f", temperature);
        sle_uart_client_send_data((uint8_t *)transmit_message, sizeof(transmit_message), 0);
    }
    else if (data->data[0] == 'A')
    {
        memcpy_s(transmit_message, 30, data->data, sizeof(data->data));
        sle_uart_client_send_data((uint8_t *)transmit_message, sizeof(transmit_message), 1);
    }
}

void sle_uart_indication_cb(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
                            errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);
    osal_printk("sle uart recived data : %s\r\n", data->data);
}

uint8_t ImageBW[27200];
uint8_t ImageR[27200];
uint8_t busy_flag = 0;
char degree_string[30] = {0};
char water_string[30] = {0};
char light_string[30] = {0};
float StartTime = 12.58; // 这个设置成程序开始的时间
float time = 0.0;
uint8_t weather = 0; // 用于判断当前天气显示
uint8_t warning = 0; // 用于判断当前警告信息显示
UINT32 seconds;
uint32_t minutes = 1;

void init_gpio(void)
{
    // LED
    uapi_pin_set_mode(2, PIN_MODE_0);
    uapi_gpio_set_dir(2, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(2, GPIO_LEVEL_LOW);
    // SDA
    uapi_pin_set_mode(9, 0);
    uapi_gpio_set_dir(9, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(9, GPIO_LEVEL_HIGH);
    // SCL
    uapi_pin_set_mode(7, 0);
    uapi_gpio_set_dir(7, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(7, GPIO_LEVEL_HIGH);
    // CS
    uapi_pin_set_mode(8, 0);
    uapi_gpio_set_dir(8, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(8, GPIO_LEVEL_HIGH);

    // BUSY引脚初始化
    uapi_pin_set_mode(13, 0);
    uapi_pin_set_pull(13, PIN_PULL_TYPE_UP);
    uapi_gpio_set_dir(13, GPIO_DIRECTION_INPUT);
    // DC引脚初始化
    uapi_pin_set_mode(10, 0);
    uapi_gpio_set_dir(10, GPIO_DIRECTION_OUTPUT);
    // CS = High (not selected)
    uapi_gpio_set_val(8, GPIO_LEVEL_HIGH);

    // Reset the EPD
    uapi_pin_set_mode(14, 0);
    uapi_gpio_set_dir(14, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(14, GPIO_LEVEL_LOW);
    osal_mdelay(100);
    uapi_gpio_set_val(14, GPIO_LEVEL_HIGH);
    osal_mdelay(100);
}

void draw_main_scene(void)
{
    if (time - (int)time >= 0.60)
    {
        time = time + 0.4;
    }
    // 显示文字的y坐标好像不能太小，原因未知
    // y坐标向下递增，x坐标向右递增
    Paint_SelectImage(ImageBW);
    Paint_Clear(WHITE);
    EPD_ShowPicture(0, 0, 800, 272, UI_pic, BLACK); // 显示背景图片
    EPD_ShowWatch(270, 20, time, 4, 2, 48, BLACK);  // 显示当前时间
    // 显示天气
    if (weather == 0)
    {
        // 统一为64宽度，50高度，坐标450，20
        EPD_ShowPicture(450, 20, 64, 50, gImage_weather_cloudy, BLACK);
    }
    else if (weather == 1)
    {
        EPD_ShowPicture(450, 20, 64, 50, gImage_weather_snowy, BLACK);
    }
    else if (weather == 2)
    {
        EPD_ShowPicture(450, 20, 64, 50, gImage_weather_rainy, BLACK);
    }
    else if (weather == 3)
    {
        EPD_ShowPicture(450, 20, 64, 50, gImage_weather_sunny, BLACK);
    }
    else if (weather == 4)
    {
        EPD_ShowPicture(450, 20, 64, 50, gImage_weather_overcast, BLACK);
    }
    else if (weather == 5)
    {
        EPD_ShowPicture(450, 20, 64, 50, gImage_weather_thunder, BLACK);
    }

    Paint_SelectImage(ImageR);
    Paint_Clear(BLACK);
    sprintf(degree_string, "%.2f", temperature);
    EPD_ShowString(145, 105, (uint8_t *)degree_string, 48, WHITE);
    sprintf(water_string, "%.2f", humidity);
    EPD_ShowString(145, 205, (uint8_t *)water_string, 48, WHITE);
    sprintf(light_string, "%d", light);
    EPD_ShowString(500, 205, (uint8_t *)light_string, 48, WHITE);
    // 显示警告信息
    if (warning == 0)
    {
        // 统一为280宽度，85高度，坐标400，85
        EPD_ShowPicture(480, 85, 280, 85, gImage_warning_common, WHITE);
    }

    EPD_Display(ImageBW, ImageR);
}

static void *EPD_task(const char *arg)
{
    unused(arg);
    time = StartTime;

    sle_uart_client_init(sle_uart_notification_cb, sle_uart_indication_cb);

    // 初始化引脚
    init_gpio();

    // 创建画布，其中BW是黑白，R是红色
    Paint_NewImage(ImageBW, EPD_W, EPD_H, 0, WHITE);
    Paint_Clear(WHITE);
    Paint_NewImage(ImageR, EPD_W, EPD_H, 0, WHITE);
    Paint_Clear(WHITE);

    // 清屏
    osal_printk("Init EPD Start.\r\n");
    EPD_Init();
    osal_printk("Clear Display Start.\r\n");
    EPD_Display_Clear();
    osal_printk("Update Start.\r\n");
    EPD_Update();
    osal_msleep(100);

    EPD_Init();

    // 初始化大概21s

    while (1)
    {
        seconds = LOS_TickCountGet();
        if (seconds > minutes * 6000)
        {
            time += ((int)(seconds / 6000) - minutes) * 0.01;
            minutes = (int)(seconds / 6000);
        }
        osal_printk("time:%ld\r\n", seconds);

        draw_main_scene();
        EPD_Update();

        for (uint8_t i = 0; i < 41; i++) // 其他工作的执行时间大概18.6s
        {
            osal_msleep(1000);
            uapi_gpio_toggle(2);
        }
        osal_msleep(400); // 三分钟刷新一次
    }

    return NULL;
}

static void EPD_entry(void)
{
    osThreadAttr_t attr;

    attr.name = "EPDTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = EPD_TASK_STACK_SIZE;
    attr.priority = EPD_TASK_PRIO;

    if (osThreadNew((osThreadFunc_t)EPD_task, NULL, &attr) == NULL)
    {
        /* Create task fail. */
    }
}

app_run(EPD_entry);