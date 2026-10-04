#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <rtthread.h>

/* 帧格式：AA 55 | 设备地址 | 功能码 | 数据长度 | 数据 | 校验和 | 0D 0A */
#define FRAME_HEAD1     0xAA
#define FRAME_HEAD2     0x55
#define FRAME_TAIL1     0x0D
#define FRAME_TAIL2     0x0A
#define DEVICE_ADDR     0x01

/* 功能码 */
#define CMD_REPORT_DATA     0x01    /* 上报实时数据 */
#define CMD_REPORT_ALARM    0x02    /* 上报告警 */
#define CMD_SET_THRESHOLD   0x03    /* 设置阈值 */

void protocol_init(void);
void protocol_report_data(float temp, float humi, float smoke, uint8_t state);
void protocol_report_alarm(uint8_t alarm_type, float value);

#endif
