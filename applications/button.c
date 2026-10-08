#include "button.h"
#include "hal.h"

#define KEY_FILTER_MS   20   /* 消抖时间 */

static key_event_cb_t event_cb = RT_NULL;
static rt_uint8_t key_last[3] = {0};

static void button_scan(void *param) {
    for (int i = 0; i < 3; i++) {
        rt_bool_t now = hal_key_read(i);
        if (now && !key_last[i]) {
            /* 按下消抖：第一次检测到不处理，20ms后再确认 */
            key_last[i] = 1;
        } else if (now && key_last[i] == 1) {
            /* 确认按下，触发回调 */
            key_last[i] = 2;
            if (event_cb) event_cb(i);
        } else if (!now) {
            key_last[i] = 0;
        }
    }
}

void button_init(key_event_cb_t cb) {
    event_cb = cb;
    hal_key_init();

    /* 创建周期定时器，20ms扫描一次 */
    static rt_timer_t timer;
    timer = rt_timer_create("key_scan", button_scan, RT_NULL,
                            KEY_FILTER_MS, RT_TIMER_FLAG_PERIODIC);
    rt_timer_start(timer);
}
