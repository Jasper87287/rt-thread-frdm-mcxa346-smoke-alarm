#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include <rtdevice.h>
#include "hal.h"
#include "app_config.h"
#include "display.h"
#include "smoke_alarm.h"
#include "fault.h"
#include "board.h"
#include "fsl_lpi2c.h"
#include "OLED_Font.h"
#include "fsl_lpadc.h"
#include "fsl_clock.h"
#include "fsl_port.h"
extern void rt_hw_us_delay(rt_uint32_t us);


/* ========== 统一提前定义：OLED引脚宏（GPIO2端口，所有I2C函数共用 ?========== */
#define SCL_PIN      5    // P2_5
#define SDA_PIN      7    // P2_7
#define SCL_HIGH()   do { GPIO2->PDDR |= (1U << SCL_PIN); GPIO2->PSOR = (1U << SCL_PIN); } while(0)
#define SCL_LOW()    do { GPIO2->PDDR |= (1U << SCL_PIN); GPIO2->PCOR = (1U << SCL_PIN); } while(0)
#define SDA_HIGH()   do { GPIO2->PDDR |= (1U << SDA_PIN); GPIO2->PSOR = (1U << SDA_PIN); } while(0)
#define SDA_LOW()    do { GPIO2->PDDR |= (1U << SDA_PIN); GPIO2->PCOR = (1U << SDA_PIN); } while(0)
#define SDA_READ()   ((GPIO2->PDIR & (1U << SDA_PIN)) ? 1 : 0)

/* ========================================
   1. 继电器控制命 ?
   用法：relay <fan|light|buzzer> <on|off>
   示例：relay fan on
======================================== */
static void cmd_relay(int argc, char **argv) {
    if (argc < 3) {
        rt_kprintf("usage: relay <fan|light|buzzer> <on|off>\n");
        return;
    }
    relay_t ch;
    if (rt_strcmp(argv[1], "fan") == 0)      ch = RELAY_FAN;
    else if (rt_strcmp(argv[1], "light") == 0) ch = RELAY_LIGHT;
    else if (rt_strcmp(argv[1], "buzzer") == 0) ch = RELAY_BUZZER;
    else {
        rt_kprintf("unknown device: %s\n", argv[1]);
        return;
    }
    rt_bool_t on = (rt_strcmp(argv[2], "on") == 0) ? RT_TRUE : RT_FALSE;
    hal_relay_set(ch, on);
    rt_kprintf("relay %s -> %s\n", argv[1], on ? "ON" : "OFF");
}
MSH_CMD_EXPORT(cmd_relay, relay control: relay <fan|light|buzzer> <on|off>);

/* ========================================
   2. 传感器模拟命令（仿真模式用）
   用法：set_temp 35    设置温度
        set_smoke 25  设置烟雾
        sim_auto     恢复随机
======================================== */
#if SIM_MODE_ENABLE
static void cmd_set_temp(int argc, char **argv) {
    if (argc < 2) { rt_kprintf("usage: set_temp <value>\n"); return; }
    float val = atof(argv[1]);
    hal_sim_set_temp(val);
}
MSH_CMD_EXPORT(cmd_set_temp, set sim temperature: set_temp <value>);

static void cmd_set_smoke(int argc, char **argv) {
    if (argc < 2) { rt_kprintf("usage: set_smoke <value>\n"); return; }
    float val = atof(argv[1]);
    hal_sim_set_smoke(val);
}
MSH_CMD_EXPORT(cmd_set_smoke, set sim smoke: set_smoke <value>);

static void cmd_sim_auto(int argc, char **argv) {
    hal_sim_reset_auto();
}
MSH_CMD_EXPORT(cmd_sim_auto, back to auto random mode);
#endif

/* ========================================
   3. 显示控制命令
   用法：page <0|1|2>  0=主页 ?1=告警 2=故障
======================================== */
static void cmd_page(int argc, char **argv) {
    if (argc < 2) { rt_kprintf("usage: page <0|1|2>\n"); return; }
    int id = atoi(argv[1]);
    display_switch_page(id);
    rt_kprintf("switch to page %d\n", id);
}
MSH_CMD_EXPORT(cmd_page, switch display page: page <0|1|2>);

/* ========================================
   4. 故障模拟命令
   用法：fault_sim  切换传感器故障状 ?
======================================== */
static void cmd_fault_sim(int argc, char **argv) {
    fault_sim_toggle();
}
MSH_CMD_EXPORT(cmd_fault_sim, toggle sensor fault simulation);

/* ========================================
   5. 告警记录查询命令
   用法：alarm_list  查看所有历史告 ?
        alarm_clear 清除所有告 ?
======================================== */
static void cmd_alarm_list(int argc, char **argv) {
    uint8_t cnt = fault_get_alarm_count();
    rt_kprintf("=== alarm records (%d) ===\n", cnt);
    for (int i = 0; i < cnt; i++) {
        alarm_record_t *r = fault_get_alarm(i);
        rt_kprintf("[%d] type=%d  val=%.1f  tick=%d\n",
                   i, r->type, r->value, r->timestamp);
    }
}
MSH_CMD_EXPORT(cmd_alarm_list, list all alarm records);

static void cmd_alarm_clear(int argc, char **argv) {
    fault_clear_all();
}
MSH_CMD_EXPORT(cmd_alarm_clear, clear all alarm records);

/* ========================================
   6. 系统信息命令
   用法：sys_info  查看系统运行信息
======================================== */
static void cmd_sys_info(int argc, char **argv) {
    rt_kprintf("=== system info ===\n");
    rt_kprintf("temp warn: %.1f  critical: %.1f\n", TEMP_WARN, TEMP_CRITICAL);
    rt_kprintf("smoke warn: %.1f\n", SMOKE_WARN);
    rt_kprintf("sample period: %dms\n", SAMPLE_PERIOD);
}
MSH_CMD_EXPORT(cmd_sys_info, show system config info);

static void cmd_pin_test(int argc, char **argv)
{
    if (argc < 3) {
        rt_kprintf("usage: pin_test <port> <pin>  e.g. pin_test 2 9\n");
        return;
    }
    int port = atoi(argv[1]);
    int pin  = atoi(argv[2]);
    
    GPIO_Type *gpio_base;
    switch(port) {
        case 0: gpio_base = GPIO0; break;
        case 1: gpio_base = GPIO1; break;
        case 2: gpio_base = GPIO2; break;
        case 3: gpio_base = GPIO3; break;
        default: rt_kprintf("port %d invalid\n", port); return;
    }
    // 配置为数字输出，默认低电 ?
    gpio_pin_config_t pin_cfg = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic = 0U
    };
    GPIO_PinInit(gpio_base, pin, &pin_cfg);
    
    // 翻转3次，观察灯闪 ?
    for (int i = 0; i < 3; i++) {
        gpio_base->PSOR = 1U << pin;    // 置位寄存器：输出高电 ?
        rt_thread_mdelay(500);
        gpio_base->PCOR = 1U << pin;    // 清零寄存器：输出低电 ?
        rt_thread_mdelay(500);
    }
    rt_kprintf("pin P%d_%d test finish\n", port, pin);
}
MSH_CMD_EXPORT(cmd_pin_test, test gpio output: pin_test <port> <pin>);

// I2C通用微秒延时
static void i2c_delay_us(uint32_t us)
{
    volatile uint32_t cnt;
    while (us--) {
        cnt = 12;
        while (cnt--);
    }
}

static void cmd_i2c_scan(int argc, char **argv)
{
    // 初始状态：SDA、SCL均释放（高电平）
    SDA_HIGH();
    SCL_HIGH();
    i2c_delay_us(10);
    rt_kprintf("Open-drain I2C scanning (0x08~0x77)...\n");
    uint8_t found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++)
    {
        // 起始信号
        SDA_HIGH();
        SCL_HIGH();
        i2c_delay_us(5);
        SDA_LOW();
        i2c_delay_us(5);
        SCL_LOW();
        i2c_delay_us(5);
        // 发送地址+写位
        uint8_t data = addr << 1;
        for (uint8_t i = 0; i < 8; i++) {
            if (data & 0x80) SDA_HIGH();
            else SDA_LOW();
            data <<= 1;
            i2c_delay_us(2);
            SCL_HIGH();
            i2c_delay_us(2);
            SCL_LOW();
            i2c_delay_us(2);
        }
        // 读取ACK
        SDA_HIGH();
        i2c_delay_us(2);
        SCL_HIGH();
        i2c_delay_us(2);
        uint8_t ack = (SDA_READ() == 0) ? 1 : 0;
        SCL_LOW();
        i2c_delay_us(2);
        // 停止信号
        SDA_LOW();
        i2c_delay_us(5);
        SCL_HIGH();
        i2c_delay_us(5);
        SDA_HIGH();
        i2c_delay_us(10);
        if (ack) {
            rt_kprintf("Found device at 0x%02X\n", addr);
            found++;
        }
    }
    if (found == 0)
        rt_kprintf("No I2C device found\n");
    else
        rt_kprintf("Total %d device(s) found\n", found);
}
MSH_CMD_EXPORT(cmd_i2c_scan, scan i2c bus with open-drain mode);

static void cmd_p18_test(int argc, char **argv)
{
    GPIO1->PDDR |= 1U << 8;
    if (argv[1][0] == '1') {
        GPIO1->PSOR = 1U << 8;
        rt_kprintf("P1_8 output high\n");
    } else {
        GPIO1->PCOR = 1U << 8;
        rt_kprintf("P1_8 output low\n");
    }
}
MSH_CMD_EXPORT(cmd_p18_test, test P1_8 output: 1=high 0=low);

/* ========================================
 *  OLED 软件模拟I2C驱动（GPIO2端口，已验证点亮 ?
 *  SCL=P2_5(J4-A1), SDA=P2_7(J4-A2)
 * ======================================== */
/* OLED专用微秒级延 ?*/
static void oled_delay_us(uint32_t us)
{
    volatile uint32_t cnt;
    while (us--) {
        cnt = 20;
        while (cnt--);
    }
}

/* I2C 起始信号 */
static void OLED_I2C_Start(void)
{
    SDA_HIGH();
    SCL_HIGH();
    oled_delay_us(5);
    SDA_LOW();
    oled_delay_us(5);
    SCL_LOW();
    oled_delay_us(5);
}

/* I2C 停止信号 */
static void OLED_I2C_Stop(void)
{
    SDA_LOW();
    oled_delay_us(5);
    SCL_HIGH();
    oled_delay_us(5);
    SDA_HIGH();
    oled_delay_us(10);
}

/* I2C 发送一个字 ?*/
static void OLED_I2C_SendByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (byte & 0x80) SDA_HIGH();
        else SDA_LOW();
        byte <<= 1;
        oled_delay_us(2);
        SCL_HIGH();
        oled_delay_us(2);
        SCL_LOW();
        oled_delay_us(2);
    }
    /* 跳过ACK应答时钟 */
    SCL_HIGH();
    oled_delay_us(2);
    SCL_LOW();
    oled_delay_us(2);
}

/* 写命 ?*/
static void OLED_WriteCommand(uint8_t cmd)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);   /* 设备写地址 ?x3C << 1 */
    OLED_I2C_SendByte(0x00);   /* 命令标识 */
    OLED_I2C_SendByte(cmd);
    OLED_I2C_Stop();
}

/* 写数 ?*/
static void OLED_WriteData(uint8_t data)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);
    OLED_I2C_SendByte(0x40);   /* 数据标识 */
    OLED_I2C_SendByte(data);
    OLED_I2C_Stop();
}

/* 清屏 */
static void OLED_Clear(void)
{
    for (uint8_t j = 0; j < 8; j++)
    {
        OLED_WriteCommand(0xB0 | j);
        OLED_WriteCommand(0x10);
        OLED_WriteCommand(0x00);
        for(uint8_t i = 0; i < 128; i++)
        {
            OLED_WriteData(0x00);
        }
    }
}

/* 显示单个字符 */
static void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    OLED_WriteCommand(0xB0 | ((Line - 1) * 2));
    OLED_WriteCommand(0x10 | (((Column - 1) * 8 & 0xF0) >> 4));
    OLED_WriteCommand(0x00 | ((Column - 1) * 8 & 0x0F));
    for (uint8_t i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Char - ' '][i]);
    }
    
    OLED_WriteCommand(0xB0 | ((Line - 1) * 2 + 1));
    OLED_WriteCommand(0x10 | (((Column - 1) * 8 & 0xF0) >> 4));
    OLED_WriteCommand(0x00 | ((Column - 1) * 8 & 0x0F));
    for (uint8_t i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);
    }
}

/* ===== HAL层全局接口（供display模块调用 ?===== */
rt_err_t hal_oled_init(void)
{
    /* OLED 完整初始化序 ?*/
    OLED_WriteCommand(0xAE);
    OLED_WriteCommand(0xD5);
    OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xA8);
    OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xD3);
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0x40);
    OLED_WriteCommand(0xA1);
    OLED_WriteCommand(0xC8);
    OLED_WriteCommand(0xDA);
    OLED_WriteCommand(0x12);
    OLED_WriteCommand(0x81);
    OLED_WriteCommand(0xCF);
    OLED_WriteCommand(0xD9);
    OLED_WriteCommand(0xF1);
    OLED_WriteCommand(0xDB);
    OLED_WriteCommand(0x30);
    OLED_WriteCommand(0xA4);
    OLED_WriteCommand(0xA6);
    OLED_WriteCommand(0x8D);
    OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xAF);
    
    OLED_Clear();
    rt_kprintf("[HAL] OLED init done\n");
    return RT_EOK;
}

rt_err_t hal_oled_clear(void)
{
    OLED_Clear();
    return RT_EOK;
}

rt_err_t hal_oled_show_string(uint8_t line, const char *str)
{
    uint8_t col = 1;
    while (*str && col <= 16)
    {
        OLED_ShowChar(line, col, *str);
        str++;
        col++;
    }
    return RT_EOK;
}

/* 串口OLED测试命令 */
static void cmd_oled_test(int argc, char **argv)
{
    rt_kprintf("OLED test start...\n");
    
    /* OLED 完整初始化序 ?*/
    OLED_WriteCommand(0xAE);
    OLED_WriteCommand(0xD5);
    OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xA8);
    OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xD3);
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0x40);
    OLED_WriteCommand(0xA1);
    OLED_WriteCommand(0xC8);
    OLED_WriteCommand(0xDA);
    OLED_WriteCommand(0x12);
    OLED_WriteCommand(0x81);
    OLED_WriteCommand(0xCF);
    OLED_WriteCommand(0xD9);
    OLED_WriteCommand(0xF1);
    OLED_WriteCommand(0xDB);
    OLED_WriteCommand(0x30);
    OLED_WriteCommand(0xA4);
    OLED_WriteCommand(0xA6);
    OLED_WriteCommand(0x8D);
    OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xAF);
    
    OLED_Clear();
    
    /* 测试显示 Hello */
    OLED_ShowChar(1, 1, 'H');
    OLED_ShowChar(1, 2, 'e');
    OLED_ShowChar(1, 3, 'l');
    OLED_ShowChar(1, 4, 'l');
    OLED_ShowChar(1, 5, 'o');
    
    rt_kprintf("OLED test done, please check screen\n");
}
MSH_CMD_EXPORT(cmd_oled_test, oled display test);

/* ========================================
 *  按键+蜂鸣器驱 ?
 * ======================================== */
/* ========== 引脚与参数配 ?========== */
#define KEY_PORT         GPIO0   // SW3对应GPIO0
#define KEY_PIN          6       // P0_6 板载SW3
#define BUZZER_PIN       20      // P2_20 蜂鸣器输 ?
#define KEY_SCAN_MS      10      // 扫描周期 ?0ms
#define DEBOUNCE_TIMES   3       // 消抖次数 ?*10=30ms
#define LONG_PRESS_TIMES 100     // 长按阈值：100*10=1 ?

/* 按键状态机 */
typedef enum {
    KEY_RELEASED = 0,    // 松开状 ?
    KEY_DEBOUNCE,        // 消抖 ?
    KEY_PRESSED,         // 确认按下
    KEY_LONG_TRIGGERED   // 长按已触 ?
} key_state_t;

static key_state_t g_key_state = KEY_RELEASED;
static uint16_t    g_key_timer = 0;
static uint8_t     g_buzzer_active = 0;

/* Stage 3 glue functions are defined after the ADC driver below. */
static void smoke_stage3_init(void);
static void smoke_stage3_tick(void);
static void smoke_stage3_short_press(void);
static void smoke_stage3_long_press(void);

static void hal_buzzer_init(void)
{
    GPIO2->PDDR |= (1U << BUZZER_PIN);
    GPIO2->PSOR = (1U << BUZZER_PIN);
}
static void hal_buzzer_start(void)
{
    GPIO2->PCOR = (1U << BUZZER_PIN);  // 拉低电平，蜂鸣器 ?
    g_buzzer_active = 1;
}

static void hal_buzzer_stop(void)
{
    GPIO2->PSOR = (1U << BUZZER_PIN);  // 拉高电平，蜂鸣器 ?
    g_buzzer_active = 0;
}

/* ========== 按键底层驱动（板载SW3，库函数配置 ?========== */
static rt_err_t key_hw_init(void)
{
    gpio_pin_config_t pin_cfg = {
        .pinDirection = kGPIO_DigitalInput,
        .outputLogic = 0U
    };
    GPIO_PinInit(KEY_PORT, KEY_PIN, &pin_cfg);
    return RT_EOK;
}

/* 读取按键原始值：1=按下（低电平有效），0=松开 */
static uint8_t hal_key_read_raw(void)
{
    return (KEY_PORT->PDIR & (1U << KEY_PIN)) ? 0 : 1;
}
/* ========== 功能映射：事件处 ?========== */
/* 单击事件：循环切换页 ?*/
static void key_on_single_click(void)
{
    smoke_stage3_short_press();
}

/* 长按事件：告警消音（关闭蜂鸣器，保留告警页面 ?*/
static void key_on_long_press(void)
{
    smoke_stage3_long_press();
}

/* ========== 按键扫描线程（核心） ========== */
static void key_scan_thread_entry(void *param)
{
    key_hw_init();
    hal_buzzer_init();
    smoke_stage3_init();
    rt_kprintf("[HAL] key & buzzer init ok\n");

    while (1)
    {
        uint8_t key_raw = hal_key_read_raw();

        switch (g_key_state)
        {
            /* 状 ?：松开状态，检测到按下进入消抖 */
            case KEY_RELEASED:
                if (key_raw == 1)
                {
                    g_key_state = KEY_DEBOUNCE;
                    g_key_timer = 0;
                }
                break;

            /* 状 ?：消抖中，连 ?次检测到按下才确 ?*/
            case KEY_DEBOUNCE:
                g_key_timer++;
                if (key_raw == 1)
                {
                    if (g_key_timer >= DEBOUNCE_TIMES)
                    {
                        g_key_state = KEY_PRESSED;
                        g_key_timer = 0;  // 重置计时器，开始长按计 ?
                    }
                }
                else
                {
                    g_key_state = KEY_RELEASED;  // 抖动，回到松开
                }
                break;

            /* 状 ?：已按下，计时判断长 ?单击 */
            case KEY_PRESSED:
                g_key_timer++;
                /* 达到长按阈值：触发长按事件 */
                if (g_key_timer >= LONG_PRESS_TIMES)
                {
                    key_on_long_press();
                    g_key_state = KEY_LONG_TRIGGERED;
                }
                /* 提前松开：触发单击事 ?*/
                else if (key_raw == 0)
                {
                    key_on_single_click();
                    g_key_state = KEY_RELEASED;
                }
                break;

            /* 状 ?：长按已触发，等待按键松开 */
            case KEY_LONG_TRIGGERED:
                if (key_raw == 0)
                {
                    g_key_state = KEY_RELEASED;
                }
                break;
        }

        smoke_stage3_tick();
        rt_thread_mdelay(KEY_SCAN_MS);
    }
}

/* ========== 开机自动启动按键线 ?========== */
static int key_app_init(void)
{
    rt_thread_t tid = rt_thread_create(
        "key_scan",
        key_scan_thread_entry,
        RT_NULL,
        1024,   // 栈大 ?
        13,     // 优先级，比显示线程低
        10
    );
    if (tid) rt_thread_startup(tid);
    return RT_EOK;
}
INIT_APP_EXPORT(key_app_init);

/* ========== 串口调试命令 ========== */
static void cmd_beep(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("用法：beep [on|off]\n");
        return;
    }
    if (strcmp(argv[1], "on") == 0)
    {
        hal_buzzer_start();
        rt_kprintf("蜂鸣器开启\n");
    }
    else
    {
        hal_buzzer_stop();
        rt_kprintf("蜂鸣器关闭\n");
    }
}
MSH_CMD_EXPORT(cmd_beep, buzzer test);


/* 按键电平测试命令：输 ?key_test，测SW3(P0_6)，持 ?0 ?*/
static void cmd_key_test(int argc, char **argv)
{
    gpio_pin_config_t pin_cfg = {
        .pinDirection = kGPIO_DigitalInput,
        .outputLogic = 0U
    };
    GPIO_PinInit(GPIO0, 6, &pin_cfg);
    
    rt_kprintf("SW3(P0_6) key level test, 10 seconds...\n");
    rt_kprintf("released=high, pressed=low\n");
    
    for(int i = 0; i < 20; i++)
    {
        uint8_t level = (GPIO0->PDIR & (1U << 6)) ? 1 : 0;
        rt_kprintf("[%2d/20] 电平: %d\n", i+1, level);
        rt_thread_mdelay(500);
    }
    rt_kprintf("测试结束\n");
}
MSH_CMD_EXPORT(cmd_key_test, test SW3 key level);


/* 烟雾传感器DO测试命令：输 ?smoke_test，持 ?0 ?*/
static void cmd_smoke_test(int argc, char **argv)
{
    gpio_pin_config_t pin_cfg = {
        .pinDirection = kGPIO_DigitalInput,
        .outputLogic = 0U
    };
    GPIO_PinInit(GPIO2, 2, &pin_cfg);
    
    rt_kprintf("Smoke sensor DO test, 10 seconds...\n");
    rt_kprintf("no smoke=high | smoke=low\n");
    
    for(int i = 0; i < 20; i++)
    {
        uint8_t level = (GPIO2->PDIR & (1U << 2)) ? 1 : 0;
        rt_kprintf("[%2d/20] 电平: %d  %s\n", 
                   i+1, level, level==0 ? "检测到烟雾" : "空气正常");
        rt_thread_mdelay(500);
    }
    rt_kprintf("测试结束\n");
}
MSH_CMD_EXPORT(cmd_smoke_test, smoke sensor DO test);

/* ========== 烟雾传感器ADC驱动（A0/P1_14，ADC1_CH12，无串口冲突 ?========== */
/* ========== Smoke ADC driver: A0/P1_14, ADC1_CH12 ========== */
#define SMOKE_ADC_BASE         ADC1
#define SMOKE_ADC_CH           12U
#define SMOKE_ADC_CMD          1U
#define SMOKE_ADC_TRIGGER      0U
#define SMOKE_ADC_MAX_CODE     4095U
#define SMOKE_ADC_RESULT_SHIFT 3U

static rt_bool_t g_smoke_adc_initialized = RT_FALSE;
static volatile rt_bool_t g_smoke_adc_scan_active = RT_FALSE;

static void hal_smoke_adc_init(void)
{
    lpadc_config_t adc_cfg;
    lpadc_conv_command_config_t cmd_cfg;
    lpadc_conv_trigger_config_t trigger_cfg;

    if (g_smoke_adc_initialized == RT_TRUE)
    {
        return;
    }

    PORT_SetPinMux(PORT1, 14U, kPORT_MuxAlt0);
    CLOCK_AttachClk(kFRO_HF_to_ADC);
    CLOCK_SetClockDiv(kCLOCK_DivADC, 4U);
    CLOCK_EnableClock(kCLOCK_GateADC1);
    LPADC_GetDefaultConfig(&adc_cfg);
    adc_cfg.enableAnalogPreliminary = true;
    LPADC_Init(SMOKE_ADC_BASE, &adc_cfg);

    LPADC_GetDefaultConvCommandConfig(&cmd_cfg);
    cmd_cfg.channelNumber = SMOKE_ADC_CH;
    LPADC_SetConvCommandConfig(SMOKE_ADC_BASE, SMOKE_ADC_CMD, &cmd_cfg);

    LPADC_GetDefaultConvTriggerConfig(&trigger_cfg);
    trigger_cfg.targetCommandId = SMOKE_ADC_CMD;
    trigger_cfg.enableHardwareTrigger = false;
    LPADC_SetConvTriggerConfig(SMOKE_ADC_BASE, SMOKE_ADC_TRIGGER, &trigger_cfg);

    rt_kprintf("[SMOKE] ADC clock=%u Hz\r\n", CLOCK_GetAdcClkFreq(1U));    g_smoke_adc_initialized = RT_TRUE;
}

static rt_bool_t hal_smoke_read_raw_channel(uint16_t channel, uint16_t *raw)
{
    lpadc_conv_command_config_t cmd_cfg;
    lpadc_conv_result_t result;
    uint32_t timeout = 100000U;

    if ((raw == RT_NULL) || (g_smoke_adc_initialized != RT_TRUE))
    {
        return RT_FALSE;
    }

    LPADC_GetDefaultConvCommandConfig(&cmd_cfg);
    cmd_cfg.channelNumber = channel;
    LPADC_SetConvCommandConfig(SMOKE_ADC_BASE, SMOKE_ADC_CMD, &cmd_cfg);

    LPADC_DoSoftwareTrigger(SMOKE_ADC_BASE, (1U << SMOKE_ADC_TRIGGER));

    while (timeout-- > 0U)
    {
        if (LPADC_GetConvResult(SMOKE_ADC_BASE, &result) != false)
        {
            *raw = result.convValue;
            return RT_TRUE;
        }
    }

    return RT_FALSE;
}

static rt_bool_t hal_smoke_read_raw(uint16_t *raw)
{
    return hal_smoke_read_raw_channel(SMOKE_ADC_CH, raw);
}

static float hal_smoke_raw_to_ppm(uint16_t raw)
{
    uint16_t code = (uint16_t)(raw >> SMOKE_ADC_RESULT_SHIFT);

    if (code > SMOKE_ADC_MAX_CODE)
    {
        code = SMOKE_ADC_MAX_CODE;
    }

    return ((float)code / (float)SMOKE_ADC_MAX_CODE) * 100.0f;
}

float hal_smoke_read_ppm(void)
{
    uint16_t raw;

    if (hal_smoke_read_raw(&raw) != RT_TRUE)
    {
        return 0.0f;
    }

    return hal_smoke_raw_to_ppm(raw);
}
/* ========== DHT22: data P3_7, relay IN P2_1 ========== */
#define DHT22_PIN   GET_PIN(3, 7)
#define RELAY_PIN   GET_PIN(2, 1)

static float g_dht_temperature = 0.0f;
static float g_dht_humidity = 0.0f;
static rt_bool_t g_dht_valid = RT_FALSE;
static rt_bool_t g_dht_zero_logged = RT_FALSE;
static rt_uint8_t g_dht_fail_count = 0U;
static volatile rt_bool_t g_dht_pin_test = RT_FALSE;
static rt_uint32_t g_dht_start_low_ms = 2U;
static rt_uint8_t g_dht_sensor_type = 0U;
static struct rt_mutex g_dht_mutex;
static rt_bool_t g_dht_mutex_ready = RT_FALSE;
static rt_bool_t g_relay_active = RT_FALSE;
static rt_bool_t g_telemetry_enabled = RT_FALSE;
static rt_bool_t g_temp_sim_enabled = RT_FALSE;
static float g_temp_sim_value = 25.0f;
static rt_bool_t g_smoke_sim_enabled = RT_FALSE;
static float g_smoke_sim_ppm = 10.0f;
static rt_bool_t g_temp_alarm_latched = RT_FALSE;

static void relay_set_active(rt_bool_t active)
{
    /* Most 5 V relay modules are active-low. */
    rt_pin_write(RELAY_PIN, (active == RT_TRUE) ? PIN_LOW : PIN_HIGH);
    g_relay_active = active;
}

static rt_err_t dht22_wait_level(rt_uint8_t level, rt_uint32_t timeout_us)
{
    while (timeout_us-- > 0U)
    {
        if (rt_pin_read(DHT22_PIN) == level)
        {
            return RT_EOK;
        }
        rt_hw_us_delay(1U);
    }

    return -RT_ETIMEOUT;
}

static rt_uint32_t dht22_ticks_per_us(void)
{
    rt_uint32_t ticks = (SysTick->LOAD + 1U) / (1000000U / RT_TICK_PER_SECOND);
    return (ticks == 0U) ? 1U : ticks;
}

static rt_uint32_t dht22_elapsed_ticks(rt_uint32_t start)
{
    rt_uint32_t now = SysTick->VAL;
    rt_uint32_t reload = SysTick->LOAD + 1U;

    if (now <= start)
    {
        return start - now;
    }

    return (reload - now) + start;
}
static rt_err_t dht22_read_raw(rt_uint8_t data[5])
{
    rt_base_t irq_level;
    rt_uint32_t start_ticks;
    rt_uint32_t elapsed_ticks;
    rt_uint32_t timeout_ticks;

    rt_pin_mode(DHT22_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(DHT22_PIN, PIN_LOW);
    rt_thread_mdelay(g_dht_start_low_ms);
    rt_pin_write(DHT22_PIN, PIN_HIGH);
    rt_pin_mode(DHT22_PIN, PIN_MODE_INPUT_PULLUP);
    rt_hw_us_delay(30U);

    /*
     * The high pulse is 26-28 us for bit 0 and about 70 us for bit 1.
     * Keep this short transfer atomic because RT-Thread preemption can
     * otherwise stretch the measured pulse and turn every bit into 1.
     */
    irq_level = rt_hw_interrupt_disable();
    timeout_ticks = dht22_ticks_per_us() * 150U;

    if (dht22_wait_level(PIN_LOW, 150U) != RT_EOK)
    {
        rt_hw_interrupt_enable(irq_level);
        return -RT_ETIMEOUT;
    }
    if (dht22_wait_level(PIN_HIGH, 150U) != RT_EOK)
    {
        rt_hw_interrupt_enable(irq_level);
        return -RT_ETIMEOUT;
    }

    for (rt_uint32_t i = 0U; i < 40U; i++)
    {
        if (dht22_wait_level(PIN_LOW, 150U) != RT_EOK)
        {
            rt_hw_interrupt_enable(irq_level);
            return -RT_ETIMEOUT;
        }

        if (dht22_wait_level(PIN_HIGH, 150U) != RT_EOK)
        {
            rt_hw_interrupt_enable(irq_level);
            return -RT_ETIMEOUT;
        }

        start_ticks = SysTick->VAL;
        elapsed_ticks = 0U;
        while (rt_pin_read(DHT22_PIN) == PIN_HIGH)
        {
            elapsed_ticks = dht22_elapsed_ticks(start_ticks);
            if (elapsed_ticks >= timeout_ticks)
            {
                rt_hw_interrupt_enable(irq_level);
                return -RT_ETIMEOUT;
            }
        }

        data[i / 8U] <<= 1;
        if (elapsed_ticks > (dht22_ticks_per_us() * 40U))
        {
            data[i / 8U] |= 1U;
        }
    }

    rt_hw_interrupt_enable(irq_level);

    if ((rt_uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
    {
        rt_kprintf("[DHT22] checksum error raw=%02X %02X %02X %02X %02X\n",
                   data[0], data[1], data[2], data[3], data[4]);
        return -RT_ERROR;
    }

    if ((data[0] | data[1] | data[2] | data[3] | data[4]) == 0U)
    {
        if (g_dht_zero_logged != RT_TRUE)
        {
            rt_kprintf("[DHT22] all-zero frame; check DATA pull-up/wiring\n");
            g_dht_zero_logged = RT_TRUE;
        }
        return -RT_ERROR;
    }

    g_dht_zero_logged = RT_FALSE;
    return RT_EOK;
}

static rt_err_t dht22_read_once(float *temperature, float *humidity, rt_uint32_t start_ms)
{
    rt_uint8_t data[5] = {0};
    rt_uint16_t raw_humidity;
    rt_uint16_t raw_temperature;
    float temp;

    if ((temperature == RT_NULL) || (humidity == RT_NULL))
    {
        return -RT_EINVAL;
    }

    g_dht_start_low_ms = start_ms;
    if (dht22_read_raw(data) != RT_EOK)
    {
        return -RT_ERROR;
    }

    if ((data[1] == 0U) && ((data[3] & 0x7FU) == 0U) &&
        (data[0] <= 100U) && ((data[2] & 0x7FU) <= 60U))
    {
        *humidity = (float)data[0];
        temp = (float)data[2];
        if ((data[3] & 0x80U) != 0U)
        {
            temp = -temp;
        }
        *temperature = temp;
        g_dht_sensor_type = 11U;
        return RT_EOK;
    }

    raw_humidity = ((rt_uint16_t)data[0] << 8) | data[1];
    raw_temperature = ((rt_uint16_t)(data[2] & 0x7FU) << 8) | data[3];

    temp = ((float)raw_temperature) / 10.0f;
    if ((data[2] & 0x80U) != 0U)
    {
        temp = -temp;
    }

    *humidity = ((float)raw_humidity) / 10.0f;
    *temperature = temp;
    g_dht_sensor_type = 22U;
    return RT_EOK;
}

static rt_err_t dht22_read(float *temperature, float *humidity)
{
    rt_err_t result;

    if (g_dht_mutex_ready == RT_TRUE)
    {
        if (rt_mutex_take(&g_dht_mutex, RT_WAITING_FOREVER) != RT_EOK)
        {
            return -RT_ERROR;
        }
    }

    result = dht22_read_once(temperature, humidity, 2U);
    if (result != RT_EOK)
    {
        rt_thread_mdelay(1000U);
        result = dht22_read_once(temperature, humidity, 20U);
    }

    if (g_dht_mutex_ready == RT_TRUE)
    {
        rt_mutex_release(&g_dht_mutex);
    }

    return result;
}

static void cmd_dht22_pin_test(int argc, char **argv)
{
    g_dht_pin_test = RT_TRUE;

    if (g_dht_mutex_ready == RT_TRUE)
    {
        rt_mutex_take(&g_dht_mutex, RT_WAITING_FOREVER);
    }

    rt_pin_mode(DHT22_PIN, PIN_MODE_INPUT_PULLUP);
    rt_thread_mdelay(2);
    rt_kprintf("[DHT22] idle level=%d\n", rt_pin_read(DHT22_PIN));

    rt_pin_mode(DHT22_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(DHT22_PIN, PIN_LOW);
    rt_kprintf("[DHT22] DATA forced LOW for 2 seconds; measure DATA-GND now\n");
    rt_thread_mdelay(2000);

    rt_pin_write(DHT22_PIN, PIN_HIGH);
    rt_pin_mode(DHT22_PIN, PIN_MODE_INPUT_PULLUP);
    rt_thread_mdelay(2);
    rt_kprintf("[DHT22] released, level=%d\n", rt_pin_read(DHT22_PIN));

    if (g_dht_mutex_ready == RT_TRUE)
    {
        rt_mutex_release(&g_dht_mutex);
    }

    g_dht_pin_test = RT_FALSE;
}
MSH_CMD_EXPORT(cmd_dht22_pin_test, test DHT22 DATA pin control);
static void cmd_smoke_sim(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("usage: smoke_sim <off|0-100>\n");
        return;
    }

    if (rt_strcmp(argv[1], "off") == 0)
    {
        g_smoke_sim_enabled = RT_FALSE;
        rt_kprintf("smoke simulation OFF\n");
        return;
    }

    g_smoke_sim_ppm = (float)atof(argv[1]);
    if (g_smoke_sim_ppm < 0.0f)
    {
        g_smoke_sim_ppm = 0.0f;
    }
    else if (g_smoke_sim_ppm > 100.0f)
    {
        g_smoke_sim_ppm = 100.0f;
    }

    g_smoke_sim_enabled = RT_TRUE;
    rt_kprintf("smoke simulation: %d.%d\n",
               (int)g_smoke_sim_ppm,
               (int)((g_smoke_sim_ppm - (float)(int)g_smoke_sim_ppm) * 10.0f + 0.5f));
}
MSH_CMD_EXPORT(cmd_smoke_sim, simulate smoke concentration: smoke_sim <off|value>);
static void cmd_relay_test(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("usage: relay_test <on|off>\n");
        return;
    }

    if (rt_strcmp(argv[1], "on") == 0)
    {
        relay_set_active(RT_TRUE);
        rt_kprintf("relay P2_1 -> ON\n");
    }
    else if (rt_strcmp(argv[1], "off") == 0)
    {
        relay_set_active(RT_FALSE);
        rt_kprintf("relay P2_1 -> OFF\n");
    }
    else
    {
        rt_kprintf("usage: relay_test <on|off>\n");
    }
}
MSH_CMD_EXPORT(cmd_relay_test, manually test relay P2_1);

static void cmd_temp_sim(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("usage: temp_sim <off|temperature_C>\n");
        return;
    }

    if (rt_strcmp(argv[1], "off") == 0)
    {
        g_temp_sim_enabled = RT_FALSE;
        rt_kprintf("temperature simulation OFF\n");
        return;
    }

    g_temp_sim_value = (float)atof(argv[1]);
    g_temp_sim_enabled = RT_TRUE;
    rt_kprintf("temperature simulation: %d.%d C\n",
               (int)g_temp_sim_value,
               (int)((g_temp_sim_value - (float)(int)g_temp_sim_value) * 10.0f + 0.5f));
}
MSH_CMD_EXPORT(cmd_temp_sim, simulate temperature: temp_sim <off|value>);
static void cmd_dht22_test(int argc, char **argv)
{
    float temperature;
    float humidity;
    rt_err_t result;

    result = dht22_read(&temperature, &humidity);
    if (result == RT_EOK)
    {
        int temp_x10 = (int)((temperature * 10.0f) + 0.5f);
        int humi_x10 = (int)((humidity * 10.0f) + 0.5f);
        rt_kprintf("[DHT%u] T=%d.%d C H=%d.%d %%RH\n",
                   (unsigned int)g_dht_sensor_type,
                   temp_x10 / 10, temp_x10 % 10,
                   humi_x10 / 10, humi_x10 % 10);
    }
    else
    {
        rt_kprintf("[DHT22] test failed (%d)\n", result);
    }
}
MSH_CMD_EXPORT(cmd_dht22_test, test DHT22 sensor);
static void cmd_dht22_test20(int argc, char **argv)
{
    g_dht_start_low_ms = 20U;
    rt_kprintf("[DHT22] using 20 ms DHT11-style start pulse\n");
    cmd_dht22_test(argc, argv);
    g_dht_start_low_ms = 2U;
}
MSH_CMD_EXPORT(cmd_dht22_test20, test DHT sensor with 20 ms start pulse);
static void dht22_thread_entry(void *param)
{
    float temperature;
    float humidity;
    rt_uint8_t log_divider = 0U;
    rt_uint8_t last_logged_type = 0U;

    rt_pin_mode(DHT22_PIN, PIN_MODE_INPUT_PULLUP);

    while (1)
    {
        if (g_dht_pin_test == RT_TRUE)
        {
            rt_thread_mdelay(100);
            continue;
        }

        if (dht22_read(&temperature, &humidity) == RT_EOK)
        {
            g_dht_fail_count = 0U;
            g_dht_temperature = temperature;
            g_dht_humidity = humidity;
            g_dht_valid = RT_TRUE;
            display_update_environment(temperature, humidity);

            if ((g_dht_sensor_type != last_logged_type) || (log_divider == 0U))
            {
                rt_kprintf("[DHT%u] T=%d.%d C H=%d.%d %%RH\n",
                           (unsigned int)g_dht_sensor_type,
                           (int)temperature,
                           (int)((temperature - (float)(int)temperature) * 10.0f + 0.5f),
                           (int)humidity,
                           (int)((humidity - (float)(int)humidity) * 10.0f + 0.5f));
            }

            last_logged_type = g_dht_sensor_type;
            if (++log_divider >= 30U)
            {
                log_divider = 0U;
            }
        }
        else
        {
            g_dht_valid = RT_FALSE;
            if (g_dht_fail_count < 255U)
            {
                g_dht_fail_count++;
            }

            if ((g_dht_fail_count <= 3U) || ((g_dht_fail_count % 30U) == 0U))
            {
                rt_kprintf("[DHT22] read failed (%u)\n", g_dht_fail_count);
            }
        }

        rt_thread_mdelay(2000);
    }
}

static void dht22_thread_start(void)
{
    if (g_dht_mutex_ready != RT_TRUE)
    {
        if (rt_mutex_init(&g_dht_mutex, "dht_bus", RT_IPC_FLAG_PRIO) == RT_EOK)
        {
            g_dht_mutex_ready = RT_TRUE;
        }
    }


    rt_thread_t tid = rt_thread_create("dht22", dht22_thread_entry,
                                       RT_NULL, 1024, 14, 10);
    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
    }
    else
    {
        rt_kprintf("[DHT22] thread create failed\n");
    }
}
/* ========== Stage 3: automatic smoke alarm glue ========== */
static smoke_alarm_t g_smoke_alarm;

static void cmd_warmup_skip(int argc, char **argv)
{
    g_smoke_alarm.warmup_ticks = 0U;
    g_smoke_alarm.trigger_count = 0U;
    g_smoke_alarm.release_count = 0U;
    rt_kprintf("smoke warmup skipped\n");
}
MSH_CMD_EXPORT(cmd_warmup_skip, skip MQ-2 warmup for bench testing);

static rt_bool_t smoke_stage3_read_ppm(float *ppm)
{
    if (ppm == RT_NULL)
    {
        return RT_FALSE;
    }

    if (g_smoke_sim_enabled == RT_TRUE)
    {
        *ppm = g_smoke_sim_ppm;
        return RT_TRUE;
    }

    if (g_smoke_adc_scan_active == RT_TRUE)
    {
        return RT_FALSE;
    }

    *ppm = hal_smoke_read_ppm();
    return RT_TRUE;
}

static rt_bool_t smoke_stage3_temperature_over_limit(void)
{
    float temperature;
    rt_bool_t valid = g_dht_valid;

    if (g_temp_sim_enabled == RT_TRUE)
    {
        temperature = g_temp_sim_value;
        valid = RT_TRUE;
    }
    else
    {
        temperature = g_dht_temperature;
    }

    if (valid != RT_TRUE)
    {
        return RT_FALSE;
    }

    if (temperature >= TEMP_CRITICAL)
    {
        g_temp_alarm_latched = RT_TRUE;
    }
    else if (temperature <= (TEMP_CRITICAL - 2.0f))
    {
        g_temp_alarm_latched = RT_FALSE;
    }

    return g_temp_alarm_latched;
}

static void smoke_stage3_set_buzzer(rt_bool_t enabled)
{
    if (enabled == RT_TRUE)
    {
        hal_buzzer_start();
    }
    else
    {
        hal_buzzer_stop();
    }
}

static void smoke_stage3_show_page(smoke_alarm_page_t page,
                                   float ppm,
                                   rt_bool_t muted)
{
    display_update_smoke(ppm);
    display_set_alarm_muted(muted);

    switch (page)
    {
    case SMOKE_ALARM_PAGE_HOME:
        display_switch_page(PAGE_MAIN);
        break;

    case SMOKE_ALARM_PAGE_ALARM:
        display_switch_page(PAGE_ALARM);
        break;

    case SMOKE_ALARM_PAGE_FAULT:
    default:
        display_switch_page(PAGE_FAULT);
        break;
    }
}

static void smoke_stage3_set_alarm_output(rt_bool_t active)
{
    relay_set_active(active);
}
static const smoke_alarm_io_t g_smoke_alarm_io =
{
    .read_ppm = smoke_stage3_read_ppm,
    .temperature_over_limit = smoke_stage3_temperature_over_limit,
    .set_buzzer = smoke_stage3_set_buzzer,
    .set_alarm_output = smoke_stage3_set_alarm_output,
    .show_page = smoke_stage3_show_page,
};

static const char *telemetry_state_name(smoke_alarm_state_t state)
{
    switch (state)
    {
    case SMOKE_ALARM_STATE_NORMAL:
        return "NORMAL";
    case SMOKE_ALARM_STATE_ACTIVE:
        return "ALARM";
    case SMOKE_ALARM_STATE_MUTED:
        return "MUTED";
    case SMOKE_ALARM_STATE_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

static void cmd_telemetry(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("usage: telemetry <on|off>\n");
        return;
    }

    if (rt_strcmp(argv[1], "on") == 0)
    {
        g_telemetry_enabled = RT_TRUE;
        rt_kprintf("telemetry ON\n");
    }
    else if (rt_strcmp(argv[1], "off") == 0)
    {
        g_telemetry_enabled = RT_FALSE;
        rt_kprintf("telemetry OFF\n");
    }
    else
    {
        rt_kprintf("usage: telemetry <on|off>\n");
    }
}
MSH_CMD_EXPORT(cmd_telemetry, telemetry <on|off>);

static void telemetry_thread_entry(void *param)
{
    float temperature;
    float humidity;
    int smoke_x10;
    int temp_x10;
    int humi_x10;
    rt_bool_t temp_valid;

    while (1)
    {
        if (g_telemetry_enabled == RT_TRUE)
        {
            smoke_x10 = (int)((smoke_alarm_get_ppm(&g_smoke_alarm) * 10.0f) + 0.5f);

            temp_valid = g_dht_valid;
            temperature = g_dht_temperature;
            humidity = g_dht_humidity;

            if (g_temp_sim_enabled == RT_TRUE)
            {
                temperature = g_temp_sim_value;
                temp_valid = RT_TRUE;
            }

            temp_x10 = (int)((temperature * 10.0f) + ((temperature >= 0.0f) ? 0.5f : -0.5f));
            humi_x10 = (int)((humidity * 10.0f) + 0.5f);

            rt_kprintf("$DATA,%u,%d,%d,%d,%s,%u,%u,%u,%u\r\n",
                       (unsigned int)rt_tick_get_millisecond(),
                       smoke_x10,
                       temp_x10,
                       humi_x10,
                       telemetry_state_name(smoke_alarm_get_state(&g_smoke_alarm)),
                       (unsigned int)((smoke_alarm_get_state(&g_smoke_alarm) == SMOKE_ALARM_STATE_ACTIVE) ||
                                      (smoke_alarm_get_state(&g_smoke_alarm) == SMOKE_ALARM_STATE_MUTED)),
                       (unsigned int)g_relay_active,
                       (unsigned int)g_buzzer_active,
                       (unsigned int)temp_valid);
        }

        rt_thread_mdelay(1000);
    }
}
static void smoke_stage3_init(void)
{
    hal_smoke_adc_init();

    rt_pin_mode(RELAY_PIN, PIN_MODE_OUTPUT);
    relay_set_active(RT_FALSE);
    dht22_thread_start();

    if (smoke_alarm_init(&g_smoke_alarm, &g_smoke_alarm_io) != RT_EOK)
    {
        rt_kprintf("[SMOKE] stage-3 init failed\n");
    }
    else
    {
        rt_thread_t tid = rt_thread_create("telemetry", telemetry_thread_entry,
                                           RT_NULL, 1024, 15, 10);
        if (tid != RT_NULL)
        {
            rt_thread_startup(tid);
        }
    }
}

static void smoke_stage3_tick(void)
{
    smoke_alarm_tick_10ms(&g_smoke_alarm);
}

static void smoke_stage3_short_press(void)
{
    smoke_alarm_on_short_press(&g_smoke_alarm);
}

static void smoke_stage3_long_press(void)
{
    smoke_alarm_on_long_press(&g_smoke_alarm);
}
static void cmd_smoke_adc_scan(int argc, char **argv)
{
    uint16_t raw;

    hal_smoke_adc_init();
    g_smoke_adc_scan_active = RT_TRUE;

    rt_kprintf("Scan ADC1 channels 0-31...\n");
    for (uint16_t channel = 0U; channel <= 31U; channel++)
    {
        if (hal_smoke_read_raw_channel(channel, &raw) == RT_TRUE)
        {
            uint16_t code = (uint16_t)(raw >> SMOKE_ADC_RESULT_SHIFT);
            int millivolts = (int)((((float)code * 3300.0f) / (float)SMOKE_ADC_MAX_CODE) + 0.5f);
            rt_kprintf("CH%02u code=%4u voltage=%u.%03uV\n",
                       channel, code,
                       (unsigned int)(millivolts / 1000),
                       (unsigned int)(millivolts % 1000));
        }
        else
        {
            rt_kprintf("CH%02u timeout\n", channel);
        }
        rt_thread_mdelay(20);
    }

    g_smoke_adc_scan_active = RT_FALSE;
}
MSH_CMD_EXPORT(cmd_smoke_adc_scan, scan ADC1 channels 0-31);
static void cmd_smoke_adc(int argc, char **argv)
{
    uint16_t raw;

    hal_smoke_adc_init();
    rt_kprintf("Smoke ADC test start (10s)...\n");
    rt_kprintf("code | voltage(V) | relative(%%)\n");
    rt_kprintf("-------------------------------\n");

    for (int i = 0; i < 20; i++)
    {
        if (hal_smoke_read_raw(&raw) != RT_TRUE)
        {
            rt_kprintf("ADC timeout\n");
            continue;
        }

        uint16_t code = (uint16_t)(raw >> SMOKE_ADC_RESULT_SHIFT);
        float ppm = hal_smoke_raw_to_ppm(raw);
        float voltage = ((float)code / (float)SMOKE_ADC_MAX_CODE) * 3.3f;

        int ppm_x10 = (int)((ppm * 10.0f) + 0.5f);
        int millivolts = (int)((voltage * 1000.0f) + 0.5f);

        rt_kprintf("%4u    |  %u.%03u    |  %d.%d\n",
                   code,
                   (unsigned int)(millivolts / 1000),
                   (unsigned int)(millivolts % 1000),
                   ppm_x10 / 10,
                   ppm_x10 % 10);
        rt_thread_mdelay(500);
    }

    rt_kprintf("test finished\n");
}
MSH_CMD_EXPORT(cmd_smoke_adc, smoke ADC concentration test);

