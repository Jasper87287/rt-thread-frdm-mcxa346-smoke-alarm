#ifndef __FAULT_H__
#define __FAULT_H__

#include <rtthread.h>
#include "hal.h"

#define ALARM_MAX_NUM   10  /* 最多缓存10条告警记录 */

typedef struct {
    uint32_t timestamp;
    uint8_t  type;
    float    value;
} alarm_record_t;

void fault_init(void);
fault_code_t fault_check_all(void);
uint8_t fault_get_alarm_count(void);
alarm_record_t* fault_get_alarm(uint8_t index);
void fault_add_alarm(uint8_t type, float value);
void fault_clear_all(void);       // 清除所有告警记录
void fault_sim_toggle(void);      // 切换传感器故障模拟
#endif
