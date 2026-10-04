#include "hal.h"
#include "app_config.h"
#include "display.h"
#include "button.h"
#include "protocol.h"
#include "fault.h"

extern rt_mq_t get_sample_mq(void);

typedef enum {
    SYS_NORMAL = 0,
    SYS_WARNING,
    SYS_CRITICAL,
} sys_state_t;



/* 鎸夐敭浜嬩欢鍥炶皟 */
static void on_key_press(uint8_t key_id) {
    static uint8_t page = 0;
    if (key_id == KEY_OK) {
        page = (page + 1) % 3;
        display_switch_page(page);
        rt_kprintf("[KEY] switch page: %d\n", page);
    }
}

static void control_thread_entry(void *param) {
    env_data_t data;
    sys_state_t cur_state = SYS_NORMAL;

    /* 娑堟姈璁℃暟鍣?*/
    static uint8_t temp_warn_cnt = 0;
    static uint8_t temp_critical_cnt = 0;
    static uint8_t smoke_warn_cnt = 0;

    hal_relay_init();
    button_init(on_key_press);
    display_init();
    protocol_init();
    fault_init();

    while (1) {
        fault_code_t fault;  // 鉁?绉诲埌浠ｇ爜鍧楁渶寮€澶村畾涔?

        if (rt_mq_recv(get_sample_mq(), &data, sizeof(env_data_t),
                       RT_WAITING_FOREVER) == RT_EOK) {

            /* 鏁呴殰鑷 鈥斺€?鍙祴鍊硷紝涓嶅畾涔?*/
            fault = fault_check_all();
            if (fault != FAULT_NONE) {
                /* 鏁呴殰闄嶇骇閫昏緫 */
                hal_relay_set(RELAY_FAN, RT_FALSE);
                hal_relay_set(RELAY_LIGHT, RT_FALSE);
                hal_relay_set(RELAY_BUZZER, RT_TRUE);
                display_switch_page(PAGE_FAULT);
                protocol_report_alarm(fault, 0);
                temp_warn_cnt = 0;
                temp_critical_cnt = 0;
                smoke_warn_cnt = 0;
                continue;
            }            // ... 鍚庣画鍘熸湁娓╁害銆佺儫闆惧垽鏂€昏緫
            /* 鏇存柊鏄剧ず鏁版嵁 */
            display_update_environment(data.temperature, data.humidity);

            /* ========== 娓╁害鍛婅娑堟姈 ========== */
            if (data.temperature >= TEMP_CRITICAL) {
                if (temp_critical_cnt < DEBOUNCE_CNT) temp_critical_cnt++;
                temp_warn_cnt = DEBOUNCE_CNT;
            } else if (data.temperature >= TEMP_WARN) {
                if (temp_warn_cnt < DEBOUNCE_CNT) temp_warn_cnt++;
                if (temp_critical_cnt > 0) temp_critical_cnt--;
            } else {
                if (temp_warn_cnt > 0) temp_warn_cnt--;
                if (temp_critical_cnt > 0) temp_critical_cnt--;
            }

            /* 璁℃暟鍣ㄨ揪鏍囨墠鎵ц鍔ㄤ綔 */
            if (temp_critical_cnt >= DEBOUNCE_CNT) {
                cur_state = SYS_CRITICAL;
                hal_relay_set(RELAY_FAN, RT_TRUE);
                hal_relay_set(RELAY_BUZZER, RT_TRUE);
                fault_add_alarm(1, data.temperature);
                display_switch_page(PAGE_ALARM);
                protocol_report_alarm(1, data.temperature);
            } else if (temp_warn_cnt >= DEBOUNCE_CNT) {
                cur_state = SYS_WARNING;
                hal_relay_set(RELAY_FAN, RT_TRUE);
                hal_relay_set(RELAY_BUZZER, RT_FALSE);
            } else {
                hal_relay_set(RELAY_FAN, RT_FALSE);
                hal_relay_set(RELAY_BUZZER, RT_FALSE);
                cur_state = SYS_NORMAL;
            }

            /* ========== 鐑熼浘鍛婅娑堟姈 ========== */
            if (data.smoke >= SMOKE_WARN) {
                if (smoke_warn_cnt < DEBOUNCE_CNT) smoke_warn_cnt++;
            } else {
                if (smoke_warn_cnt > 0) smoke_warn_cnt--;
            }

            if (smoke_warn_cnt >= DEBOUNCE_CNT) {
                hal_relay_set(RELAY_BUZZER, RT_TRUE);
                cur_state = SYS_CRITICAL;
                fault_add_alarm(2, data.smoke);
                protocol_report_alarm(2, data.smoke);
            }

            /* 鍏夌収鑱斿姩绮惧害瑕佹眰浣庯紝鏃犻渶娑堟姈 */
            hal_relay_set(RELAY_LIGHT, data.light < LIGHT_LOW);

            /* 瀹氭椂涓婃姤鏁版嵁 */
            static uint8_t report_cnt = 0;
       

            if (++report_cnt >= REPORT_INTERVAL_CNT) {
                report_cnt = 0;
                protocol_report_data(data.temperature, data.humidity, data.smoke, cur_state);
            }

            rt_kprintf("[STATE] T=%.1f H=%.1f S=%.1f state=%d\n",
                       data.temperature, data.humidity, data.smoke, cur_state);
        }
    }
}

int control_thread_init(void) {
    rt_thread_t tid = rt_thread_create("control", control_thread_entry,
                                   RT_NULL, CONTROL_THREAD_STACK, 11, 10);
    if (tid) rt_thread_startup(tid);
    return 0;
}

