#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

#include <rtthread.h>

/* ========================================
 *  1. 告警阈值配置
 * ======================================== */
#define TEMP_WARN           30.0f   // 温度警告阈值 (℃)
#define TEMP_CRITICAL       35.0f   // 温度临界阈值 (℃)
#define SMOKE_WARN          20.0f   // 烟雾告警阈值 (ppm)
#define HUMI_HIGH           70.0f   // 湿度高限阈值 (%RH)
#define LIGHT_LOW           100.0f  // 光照低限阈值 (lux)

/* ========================================
 *  2. 采样与滤波配置
 * ======================================== */
#define SAMPLE_PERIOD       1000    // 数据采集周期 (ms)
#define FILTER_ALPHA        0.3f    // 低通滤波系数 0~1，越小越平滑

/* 传感器数据合理量程（边界钳位用） */
#define TEMP_MIN            -20.0f
#define TEMP_MAX            80.0f
#define HUMI_MIN            0.0f
#define HUMI_MAX            100.0f
#define SMOKE_MIN           0.0f
#define SMOKE_MAX           500.0f
#define LIGHT_MIN           0.0f
#define LIGHT_MAX           10000.0f

/* ========================================
 *  3. 消抖与故障判定配置
 * ======================================== */
#define DEBOUNCE_CNT        5       // 告警消抖次数：连续N次超标才触发
#define FAULT_THRESHOLD     8       // 故障判定次数：连续N次异常判定故障
#define RECOVER_THRESHOLD   15      // 恢复判定次数：连续N次正常才恢复

/* ========================================
 *  4. 系统资源配置
 * ======================================== */
#define ALARM_MAX_NUM       10      // 最大缓存告警记录条数
#define SAMPLE_THREAD_STACK 1024    // 采集线程栈大小
#define CONTROL_THREAD_STACK 2048   // 控制线程栈大小
#define SAMPLE_MQ_BUF_SIZE  128     // 采集消息队列缓冲区大小

#define REPORT_INTERVAL_CNT 5       // 数据上报周期（单位：采样次数）

/* ========================================
 *  5. 串口通信配置
 * ======================================== */
#define PROTOCOL_DEVICE_ADDR 0x01   // 设备地址
#define UART_BAUDRATE       115200  // 串口波特率（硬件模式用）

/* ========================================
 *  6. 硬件引脚配置（预留，开发板到货后补充）
 * ======================================== */
// GPIO 引脚定义示例：#define PIN_RELAY_FAN  GET_PIN(1, 0)
// I2C 设备地址：#define SHT30_I2C_ADDR  0x44

/* ========================================
 *  7. 通用工具宏
 * ======================================== */
#define CLAMP(val, min, max)  do { \
    if ((val) < (min)) (val) = (min); \
    if ((val) > (max)) (val) = (max); \
} while(0)

/* ========================================
 *  6. 硬件模式开关与引脚配置
 * ======================================== */
/* 模式开关：1=仿真模式  0=真实硬件模式 */
#define SIM_MODE_ENABLE     0   // 已到硬件阶段，直接切硬件模式

/* GPIO引脚计算宏（与BSP标准一致） */
#define GET_PIN(port, pin)  ((port) * 32 + (pin))

/* ---------- 板载资源（已核对） ---------- */
#define PIN_LED_USER        GET_PIN(2, 9)    // 已确认：RGB其中一路对应P2_9
// #define PIN_LED_RED      GET_PIN(3, 18)   // 待手册进一步确认
// #define PIN_LED_GREEN    GET_PIN(3, 19)   // 待手册进一步确认
// #define PIN_LED_BLUE     GET_PIN(3, 21)   // 待手册进一步确认

// #define PIN_KEY_OK       GET_PIN(2, 3)    // SW2按键暂未找到对应引脚，后续外接按键替代

/* ---------- 外接外设GPIO ---------- */
#define PIN_RELAY_FAN       GET_PIN(1, 0)    // J2扩展引脚，备用
#define PIN_RELAY_BUZZER    GET_PIN(1, 1)    // J2扩展引脚，备用
#define PIN_RELAY_LIGHT     GET_PIN(1, 2)    // J2扩展引脚，备用

/* ---------- I2C 总线（已核对：J2接口） ---------- */
#define I2C_BUS_NAME        "i2c2"           // 对应LPI2C2，SDA=P1_8、SCL=P1_9（J2上）
#define OLED_I2C_ADDR       0x3C             // OLED屏默认地址
#define SHT30_I2C_ADDR      0x44             // SHT30默认地址

/* ---------- ADC 配置（J4接口：A0~A5） ---------- */
#define ADC_DEV_NAME        "adc0"           // 先默认adc0，后续读值验证
#define ADC_CH_A0           0                // J4-A0
#define ADC_CH_A1           1                // J4-A1
#define ADC_CH_A2           2                // J4-A2
// 烟雾、光敏后续从A0/A1里选
#define ADC_CH_SMOKE        14
#define ADC_CH_LIGHT        5

/* ADC分压校准系数 */
#define ADC_SMOKE_SCALE     1.5f             // 5V分压还原系数
#define ADC_LIGHT_SCALE     1.0f             // 3.3V供电无需分压

/* ---------- 串口 ---------- */
#define UART_DEV_NAME       "uart0"          // 板载调试串口
#define UART_BAUDRATE       115200

#endif /* __APP_CONFIG_H__ */
