#ifndef __HAL_H__       // 开头：如果没定义过这个宏，才编译下面内容
#define __HAL_H__

#include <rtthread.h>

/* ===== 下面是你原来的所有内容 ===== */
typedef struct {
    float temperature;
    float humidity;
    float smoke;
    float light;
} env_data_t;

typedef enum {
    RELAY_FAN = 0,
    RELAY_LIGHT,
    RELAY_BUZZER,
} relay_t;

rt_err_t hal_sensor_init(void);
rt_err_t hal_sensor_read(env_data_t *data);

rt_err_t hal_relay_init(void);
rt_err_t hal_relay_set(relay_t ch, rt_bool_t on);

/* OLED显示接口 */
rt_err_t hal_oled_init(void);
rt_err_t hal_oled_clear(void);
rt_err_t hal_oled_show_string(uint8_t line, const char *str);

/* 按键接口 */
typedef enum {
    KEY_OK = 0,    /* 对应板载SW2按键 */
    KEY_UP,
    KEY_DOWN,
} key_id_t;


rt_err_t hal_key_init(void);
rt_bool_t hal_key_read(key_id_t key);

/* 串口接口 */
rt_err_t hal_uart_send(uint8_t *buf, uint16_t len);  /* 补上漏掉的发送声明 */
rt_err_t hal_uart_recv(uint8_t *buf, uint16_t len, uint32_t timeout_ms);

/* 故障码定义 */
typedef enum {
    FAULT_NONE = 0,
    FAULT_SENSOR_OFFLINE,  /* 传感器离线 */
    FAULT_UART_ERR,        /* 串口通信异常 */
    FAULT_OLED_ERR,        /* 显示异常 */
} fault_code_t;
/* ===== 原来内容结束 ===== */

/* ========== 仿真调试专用接口（仅仿真模式有效） ========== */
void hal_sim_set_temp(float val);   // 手动设置温度
void hal_sim_set_smoke(float val);  // 手动设置烟雾浓度
void hal_sim_reset_auto(void);      // 恢复自动随机模式

#endif  // 结尾：对应开头的#ifndef
