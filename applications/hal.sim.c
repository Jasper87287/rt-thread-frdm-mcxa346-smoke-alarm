#include "app_config.h"
#if SIM_MODE_ENABLE  // 仿真模式下才编译本文件

#include "hal.h"
#include <stdlib.h>

/* 手动模式变量 */
static uint8_t sim_manual_mode = 0;  // 0=自动随机 1=手动固定
static float sim_manual_temp = 25.0f;
static float sim_manual_smoke = 10.0f;

/* 实现调试接口 */
void hal_sim_set_temp(float val) {
    sim_manual_temp = val;
    sim_manual_mode = 1;
    rt_kprintf("[SIM] temp fixed to %.1f\n", val);
}

void hal_sim_set_smoke(float val) {
    sim_manual_smoke = val;
    sim_manual_mode = 1;
    rt_kprintf("[SIM] smoke fixed to %.1f\n", val);
}

void hal_sim_reset_auto(void) {
    sim_manual_mode = 0;
    rt_kprintf("[SIM] back to auto random mode\n");
}
rt_err_t hal_sensor_init(void) {
    rt_kprintf("[HAL-SIM] sensor init ok\n");
    return RT_EOK;
}

rt_err_t hal_sensor_read(env_data_t *data) {
    if (sim_manual_mode) {
        // 手动模式：用设定值
        data->temperature = sim_manual_temp;
        data->humidity    = 55.0f;
        data->smoke       = sim_manual_smoke;
        data->light       = 300.0f;
    } else {
        // 自动模式：原来的随机逻辑
        data->temperature = 28.0f + (rand() % 30) / 10.0f;
        data->humidity    = 55.0f + (rand() % 200) / 10.0f;
        data->smoke       = 10.0f + (rand() % 50) / 10.0f;
        data->light       = 300.0f + (rand() % 100);
    }
    return RT_EOK;
}
static rt_bool_t relay_state[3] = {0};

rt_err_t hal_relay_init(void) {
    rt_kprintf("[HAL-SIM] relay init ok\n");
    return RT_EOK;
}

rt_err_t hal_relay_set(relay_t ch, rt_bool_t on) {
    const char *name[] = {"FAN", "LIGHT", "BUZZER"};
    if (relay_state[ch] != on) {
        relay_state[ch] = on;
        rt_kprintf("[RELAY] %s -> %s\n", name[ch], on ? "ON" : "OFF");
    }
    return RT_EOK;
}
/* ========== 模拟OLED：串口打印屏幕内容 ========== */
rt_err_t hal_oled_init(void) {
    rt_kprintf("[HAL-SIM] oled init ok (sim mode)\n");
    return RT_EOK;
}

rt_err_t hal_oled_clear(void) {
    rt_kprintf("[OLED] -------- clear screen --------\n");
    return RT_EOK;
}

rt_err_t hal_oled_show_string(uint8_t line, const char *str) {
    rt_kprintf("[OLED] line%d: %s\n", line, str);
    return RT_EOK;
}

/* ========== 模拟按键：全局变量模拟状态，msh命令控制 ========== */
static rt_bool_t key_state[3] = {0};

rt_err_t hal_key_init(void) {
    rt_kprintf("[HAL-SIM] key init ok (sim mode)\n");
    return RT_EOK;
}

rt_bool_t hal_key_read(key_id_t key) {
    return key_state[key];
}

/* msh命令：模拟按键按下，输入 key_press 0 模拟OK键 */
static void cmd_key_press(int argc, char **argv) {
    if (argc < 2) return;
    int id = atoi(argv[1]);
    if (id >= 0 && id < 3) {
        key_state[id] = 1;
        rt_thread_mdelay(50);  /* 模拟按下50ms */
        key_state[id] = 0;
        rt_kprintf("[SIM] key%d pressed\n", id);
    }
}
MSH_CMD_EXPORT(cmd_key_press, sim key press: key_press <id>);

/* ========== 模拟串口接收：暂返回空，仿真用msh模拟指令 ========== */
/* 仿真串口发送：直接打印发送内容，模拟发送成功 */
rt_err_t hal_uart_send(uint8_t *buf, uint16_t len) {
    rt_kprintf("[UART-SEND] len=%d\n", len);
    return RT_EOK;
}

rt_err_t hal_uart_recv(uint8_t *buf, uint16_t len, uint32_t timeout_ms) {
    return -RT_ETIMEOUT;
}
#endif /* SIM_MODE_ENABLE */

