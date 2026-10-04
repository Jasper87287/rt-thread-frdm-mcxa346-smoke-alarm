#include "app_config.h"
#if !SIM_MODE_ENABLE  // 硬件模式下才编译本文件

#include "hal.h"
#include <rtdevice.h>
#include "fsl_gpio.h"
// #include <drivers/adc.h>    // ADC 标准驱动头文件
// #include <drivers/i2c.h>    // I2C 标准驱动头文件
/* ========================================
 *  1. GPIO 继电器驱动
 * ======================================== */
static const uint8_t relay_port[] = {1, 1, 1};   // 对应端口号
static const uint8_t relay_pin[]  = {0, 1, 2};   // 对应引脚号

rt_err_t hal_relay_init(void)
{
    gpio_pin_config_t pin_cfg = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic = 0U
    };
    
    for (int i = 0; i < sizeof(relay_pin)/sizeof(relay_pin[0]); i++)
    {
        GPIO_PinInit(GPIO1, relay_pin[i], &pin_cfg);
    }
    rt_kprintf("[HAL] relay init ok\n");
    return RT_EOK;
}

rt_err_t hal_relay_set(relay_t ch, rt_bool_t on)
{
    if (ch >= sizeof(relay_pin)/sizeof(relay_pin[0]))
        return -RT_ERROR;

    if (on) {
        GPIO1->PSOR = 1U << relay_pin[ch];  // 置位：输出高
    } else {
        GPIO1->PCOR = 1U << relay_pin[ch];  // 清零：输出低
    }
    return RT_EOK;
}
#if 0
/* ========================================
 *  2. GPIO 按键驱动
 * ======================================== */
rt_err_t hal_key_init(void)
{
    // 上拉输入：按下为低电平
    rt_pin_mode(PIN_KEY_OK, PIN_MODE_INPUT_PULLUP);
    rt_kprintf("[HAL] key init ok\n");
    return RT_EOK;
}

rt_bool_t hal_key_read(key_id_t key)
{
    if (key != KEY_OK) return RT_FALSE;
    return (rt_pin_read(PIN_KEY_OK) == PIN_LOW) ? RT_TRUE : RT_FALSE;
}
#endif

#if 0
/* ========================================
 *  3. ADC 采集驱动（烟雾+光敏）
 * ======================================== */
static rt_adc_device_t g_adc_dev = RT_NULL;

static rt_err_t adc_hw_init(void)
{
    g_adc_dev = rt_adc_find(ADC_DEV_NAME);
    if (g_adc_dev == RT_NULL)
    {
        rt_kprintf("[HAL] ADC %s not found\n", ADC_DEV_NAME);
        return -RT_ERROR;
    }
    rt_adc_enable(g_adc_dev, ADC_CH_SMOKE);
    rt_adc_enable(g_adc_dev, ADC_CH_LIGHT);
    return RT_EOK;
}

// ADC原始值转物理量（硬件到手后校准系数）
static float adc_to_smoke(rt_uint32_t raw)
{
    // TODO: 硬件到手后根据实际模块校准
    return (float)raw * 0.1f;
}

static float adc_to_light(rt_uint32_t raw)
{
    // TODO: 硬件到手后根据光敏模块校准
    return (float)raw * 1.0f;
}
#endif

#if 0
/* ========================================
 *  4. I2C 总线驱动（OLED + SHT30）
 * ======================================== */
static struct rt_i2c_bus_device *g_i2c_bus = RT_NULL;

static rt_err_t i2c_hw_init(void)
{
    g_i2c_bus = rt_i2c_find(I2C_BUS_NAME);
    if (g_i2c_bus == RT_NULL)
    {
        rt_kprintf("[HAL] I2C %s not found\n", I2C_BUS_NAME);
        return -RT_ERROR;
    }
    return RT_EOK;
}

// I2C写寄存器通用函数
static rt_err_t i2c_write_reg(rt_uint8_t dev_addr, rt_uint8_t reg, rt_uint8_t *data, rt_uint8_t len)
{
    struct rt_i2c_msg msg;
    rt_uint8_t buf[32];

    buf[0] = reg;
    for (int i = 0; i < len; i++) buf[i+1] = data[i];

    msg.addr  = dev_addr;
    msg.flags = RT_I2C_WR;
    msg.buf   = buf;
    msg.len   = len + 1;

    return rt_i2c_transfer(g_i2c_bus, &msg, 1);
}

// I2C读寄存器通用函数
static rt_err_t i2c_read_reg(rt_uint8_t dev_addr, rt_uint8_t reg, rt_uint8_t *data, rt_uint8_t len)
{
    struct rt_i2c_msg msgs[2];

    msgs[0].addr  = dev_addr;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf   = &reg;
    msgs[0].len   = 1;

    msgs[1].addr  = dev_addr;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf   = data;
    msgs[1].len   = len;

    return rt_i2c_transfer(g_i2c_bus, msgs, 2);
}



/* ========================================
 *  6. SHT30 温湿度读取（框架）
 * ======================================== */
static rt_err_t sht30_read(float *temp, float *humi)
{
    // TODO: 硬件到手后填充SHT30读取与转换逻辑
    *temp = 25.0f;
    *humi = 50.0f;
    return RT_EOK;
}
#endif

/* ========================================
 *  7. 传感器统一接口（桩函数，硬件调通再替换）
 * ======================================== */
rt_err_t hal_sensor_init(void)
{
    rt_kprintf("[HAL] sensor init stub ok\n");
    return RT_EOK;
}

rt_err_t hal_sensor_read(env_data_t *data)
{
    // 临时返回固定值，ADC、温湿度调试好再替换
    data->temperature = 25.0f;
    data->humidity    = 50.0f;
    data->smoke       = 10.0f;
    data->light       = 300.0f;
    return RT_EOK;
}

/* ========================================
 *  9. 按键驱动（桩函数，引脚确认后填充）
 * ======================================== */
rt_err_t hal_key_init(void)
{
    rt_kprintf("[HAL] key init stub\n");
    return RT_EOK;
}

rt_bool_t hal_key_read(key_id_t key)
{
    // 暂时返回未按下，按键引脚确认后填充
    return RT_FALSE;
}

/* ========================================
 *  8. 串口通信驱动
 * ======================================== */
static rt_device_t g_uart_dev = RT_NULL;

rt_err_t hal_uart_send(rt_uint8_t *buf, rt_uint16_t len)
{
    if (g_uart_dev == RT_NULL) return -RT_ERROR;
    return rt_device_write(g_uart_dev, 0, buf, len);
}

rt_err_t hal_uart_recv(rt_uint8_t *buf, rt_uint16_t len, rt_uint32_t timeout_ms)
{
    if (g_uart_dev == RT_NULL) return -RT_ERROR;
    // 查询式接收，硬件到手后可改为中断+信号量
    return rt_device_read(g_uart_dev, 0, buf, len);
}

#endif /* !SIM_MODE_ENABLE */
