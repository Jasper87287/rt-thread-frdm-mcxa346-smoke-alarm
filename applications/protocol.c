#include "protocol.h"
#include "hal.h"

/* 计算校验和：从地址到数据区所有字节累加 */
static uint8_t calc_checksum(uint8_t *data, uint8_t len) {
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) sum += data[i];
    return sum;
}

/* 通用组包发送 */
static void send_frame(uint8_t cmd, uint8_t *data, uint8_t data_len) {
    uint8_t buf[64];
    uint8_t idx = 0;
    if (data_len > 50) return;

    buf[idx++] = FRAME_HEAD1;
    buf[idx++] = FRAME_HEAD2;
    buf[idx++] = DEVICE_ADDR;
    buf[idx++] = cmd;
    buf[idx++] = data_len;
    for (int i = 0; i < data_len; i++) buf[idx++] = data[i];
    buf[idx++] = calc_checksum(&buf[2], data_len + 3);
    buf[idx++] = FRAME_TAIL1;
    buf[idx++] = FRAME_TAIL2;

    hal_uart_send(buf, idx);
}

void protocol_report_data(float temp, float humi, float smoke, uint8_t state) {
    uint8_t data[8];
    /* 浮点数转整数放大10倍传输，节省带宽 */
    int16_t t = (int16_t)(temp * 10);
    int16_t h = (int16_t)(humi * 10);
    int16_t s = (int16_t)(smoke * 10);

    data[0] = t >> 8; data[1] = t & 0xFF;
    data[2] = h >> 8; data[3] = h & 0xFF;
    data[4] = s >> 8; data[5] = s & 0xFF;
    data[6] = state;

    send_frame(CMD_REPORT_DATA, data, 7);
}

void protocol_report_alarm(uint8_t alarm_type, float value) {
    uint8_t data[3];
    int16_t v = (int16_t)(value * 10);

    data[0] = alarm_type;
    data[1] = v >> 8; data[2] = v & 0xFF;

    send_frame(CMD_REPORT_ALARM, data, 3);
}

void protocol_init(void) {
    /* 串口初始化放在hal层，这里只做协议初始化 */
    rt_kprintf("[PROTOCOL] init ok\n");
}
