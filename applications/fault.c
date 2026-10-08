#include "fault.h"
#include "app_config.h"
static alarm_record_t alarm_list[ALARM_MAX_NUM];
static uint8_t alarm_cnt = 0;
static uint32_t tick_count = 0;

/* 模拟传感器离线检测：仿真版默认正常，可通过msh命令触发 */
static uint8_t sim_sensor_offline = 0;

static void cmd_sim_fault(int argc, char **argv) {
    sim_sensor_offline = !sim_sensor_offline;
    rt_kprintf("[SIM] sensor offline: %d\n", sim_sensor_offline);
}
MSH_CMD_EXPORT(cmd_sim_fault, sim sensor fault);


/* 故障与恢复计数器 */
static uint8_t fault_cnt = 0;
static uint8_t recover_cnt = 0;
/* 故障判定阈值：连续8次异常判定为故障 */

/* 恢复判定阈值：连续15次正常才恢复 */


fault_code_t fault_check_all(void) {
    tick_count++;

    if (sim_sensor_offline) {
        recover_cnt = 0;
        if (fault_cnt < FAULT_THRESHOLD) {
            fault_cnt++;
            return FAULT_NONE; // 未达阈值，暂不报故障
        }
        return FAULT_SENSOR_OFFLINE;
    } else {
        fault_cnt = 0;
        if (recover_cnt < RECOVER_THRESHOLD) {
            recover_cnt++;
            return FAULT_SENSOR_OFFLINE; // 未达恢复阈值，保持故障状态
        }
        return FAULT_NONE;
    }
}

void fault_add_alarm(uint8_t type, float value) {
    /* 入口参数保护 */
    if (type > 10) return;

    if (alarm_cnt < ALARM_MAX_NUM) {
        alarm_list[alarm_cnt].timestamp = tick_count;
        alarm_list[alarm_cnt].type = type;
        alarm_list[alarm_cnt].value = value;
        alarm_cnt++;
    } else {
        /* 循环覆盖最早一条 */
        for (int i = 0; i < ALARM_MAX_NUM - 1; i++) {
            alarm_list[i] = alarm_list[i + 1];
        }
        alarm_list[ALARM_MAX_NUM - 1].timestamp = tick_count;
        alarm_list[ALARM_MAX_NUM - 1].type = type;
        alarm_list[ALARM_MAX_NUM - 1].value = value;
    }
}

uint8_t fault_get_alarm_count(void) {
    return alarm_cnt;
}

alarm_record_t* fault_get_alarm(uint8_t index) {
    if (index < alarm_cnt) return &alarm_list[index];
    return RT_NULL;
}

void fault_init(void) {
    rt_kprintf("[FAULT] self-check module init ok\n");
}
/* 清除所有告警记录 */
void fault_clear_all(void) {
    alarm_cnt = 0;
    rt_kprintf("all alarm records cleared\n");
}

/* 切换传感器故障模拟状态 */
void fault_sim_toggle(void) {
    sim_sensor_offline = !sim_sensor_offline;
    rt_kprintf("[SIM] sensor offline: %s\n", sim_sensor_offline ? "ON" : "OFF");
}
