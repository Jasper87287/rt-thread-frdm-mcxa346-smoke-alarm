#include "display.h"
#include "hal.h"
#include "app_config.h"

static page_id_t cur_page = PAGE_MAIN;
static float g_temp = 25.0f;
static float g_humi = 50.0f;
static float g_smoke = 0.0f;
static rt_bool_t g_alarm_muted = RT_FALSE;
static uint8_t page_just_switched = 1;

static int display_value_x10(float value)
{
    return (int)((value * 10.0f) + ((value >= 0.0f) ? 0.5f : -0.5f));
}
static void display_thread_entry(void *param)
{
    char buf[32];

    hal_oled_init();

    while (1)
    {
        int temp_x10 = display_value_x10(g_temp);
        int humi_x10 = display_value_x10(g_humi);
        int smoke_x10 = display_value_x10(g_smoke);
        switch (cur_page)
        {
        case PAGE_MAIN:
            if (page_just_switched)
            {
                hal_oled_clear();
                page_just_switched = 0;
            }

            rt_snprintf(buf, sizeof(buf), "T:%d H:%d%%", (temp_x10 + 5) / 10, (humi_x10 + 5) / 10);
            hal_oled_show_string(1, buf);
            rt_snprintf(buf, sizeof(buf), "SMOKE:%d.%d ppm", smoke_x10 / 10, smoke_x10 % 10);
            hal_oled_show_string(2, buf);
            break;

        case PAGE_ALARM:
            if (page_just_switched)
            {
                hal_oled_clear();
                page_just_switched = 0;
            }

            rt_snprintf(buf, sizeof(buf), "SMOKE:%d.%d ppm", smoke_x10 / 10, smoke_x10 % 10);
            hal_oled_show_string(1, buf);
            hal_oled_show_string(2, (g_alarm_muted == RT_TRUE) ?
                                 "Alarm: MUTED" : "!!! ALARM !!!");
            break;

        case PAGE_FAULT:
            if (page_just_switched)
            {
                hal_oled_clear();
                hal_oled_show_string(1, "SYSTEM FAULT");
                hal_oled_show_string(2, "Check sensor");
                page_just_switched = 0;
            }
            break;

        default:
            break;
        }

        rt_thread_mdelay(200);
    }
}

void display_switch_page(page_id_t page)
{
    if (cur_page != page)
    {
        cur_page = page;
        page_just_switched = 1;
    }
}

void display_update_data(float temp, float humi, float smoke)
{
    g_temp = temp;
    g_humi = humi;
    g_smoke = smoke;
}

void display_update_environment(float temp, float humi)
{
    g_temp = temp;
    g_humi = humi;
}
void display_update_smoke(float smoke)
{
    g_smoke = smoke;
}

void display_set_alarm_muted(rt_bool_t muted)
{
    if (g_alarm_muted != muted)
    {
        g_alarm_muted = muted;
        page_just_switched = 1;
    }
}

void display_init(void)
{
    rt_thread_t tid = rt_thread_create("display", display_thread_entry,
                                       RT_NULL, 1024, 12, 10);
    if (tid)
    {
        rt_thread_startup(tid);
    }
}

page_id_t display_get_current_page(void)
{
    return cur_page;
}

void display_next_page(void)
{
    switch (cur_page)
    {
    case PAGE_MAIN:
        display_switch_page(PAGE_ALARM);
        break;
    case PAGE_ALARM:
        display_switch_page(PAGE_FAULT);
        break;
    case PAGE_FAULT:
    default:
        display_switch_page(PAGE_MAIN);
        break;
    }
}
