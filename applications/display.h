#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <rtthread.h>


typedef enum {
    PAGE_MAIN = 0,   /* 主页面：实时数据 */
    PAGE_ALARM,      /* 告警页面 */
    PAGE_FAULT,      /* 故障页面 */
} page_id_t;

void display_init(void);
void display_switch_page(page_id_t page);
void display_update_data(float temp, float humi, float smoke);
void display_update_environment(float temp, float humi);
void display_update_smoke(float smoke);
void display_set_alarm_muted(rt_bool_t muted);
void display_next_page(void);
page_id_t display_get_current_page(void);



#endif
