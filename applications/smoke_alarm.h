#ifndef SMOKE_ALARM_H
#define SMOKE_ALARM_H

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stage-3 policy, override from compiler options if needed. */
#ifndef SMOKE_ALARM_PPM_TRIGGER
#define SMOKE_ALARM_PPM_TRIGGER          50.0f
#endif

#ifndef SMOKE_ALARM_PPM_RELEASE
#define SMOKE_ALARM_PPM_RELEASE          40.0f
#endif

#ifndef SMOKE_ALARM_SAMPLE_PERIOD_MS
#define SMOKE_ALARM_SAMPLE_PERIOD_MS     100U
#endif

#ifndef SMOKE_ALARM_DISPLAY_PERIOD_MS
#define SMOKE_ALARM_DISPLAY_PERIOD_MS    200U
#endif

#ifndef SMOKE_ALARM_TRIGGER_SAMPLES
#define SMOKE_ALARM_TRIGGER_SAMPLES      3U
#endif

#ifndef SMOKE_ALARM_RELEASE_SAMPLES
#define SMOKE_ALARM_RELEASE_SAMPLES      5U
#endif

/* Low-pass filter coefficient: new = old + alpha * (raw - old). */
#ifndef SMOKE_ALARM_FILTER_ALPHA
#define SMOKE_ALARM_FILTER_ALPHA         0.25f

/* Keep the sensor quiet while the MQ-2 heater stabilizes. */
#ifndef SMOKE_ALARM_WARMUP_MS
#define SMOKE_ALARM_WARMUP_MS            180000U
#endif
#endif

typedef enum
{
    SMOKE_ALARM_PAGE_HOME = 0,
    SMOKE_ALARM_PAGE_ALARM,
    SMOKE_ALARM_PAGE_FAULT
} smoke_alarm_page_t;

typedef enum
{
    SMOKE_ALARM_STATE_NORMAL = 0,
    SMOKE_ALARM_STATE_ACTIVE,
    SMOKE_ALARM_STATE_MUTED,
    SMOKE_ALARM_STATE_FAULT
} smoke_alarm_state_t;

typedef struct
{
    /* Return RT_TRUE only when a valid 0..100 relative concentration is read. */
    rt_bool_t (*read_ppm)(float *ppm);

    /* Optional. Return RT_TRUE when the temperature channel is over limit. */
    rt_bool_t (*temperature_over_limit)(void);

    /* The adapter must handle active-low or active-high hardware polarity. */
    void (*set_buzzer)(rt_bool_t enabled);

    /* Optional. Active while the alarm is latched, independent of mute. */
    void (*set_alarm_output)(rt_bool_t active);

    /* Render or refresh one page. ppm is already filtered. */
    void (*show_page)(smoke_alarm_page_t page, float ppm, rt_bool_t muted);
} smoke_alarm_io_t;

typedef struct
{
    const smoke_alarm_io_t *io;

    smoke_alarm_state_t state;
    smoke_alarm_page_t page;

    float ppm_raw_last;
    float ppm_filtered;

    rt_uint16_t sample_ticks;
    rt_uint16_t display_ticks;
    rt_uint32_t warmup_ticks;
    rt_uint8_t trigger_count;
    rt_uint8_t release_count;
    rt_uint8_t invalid_count;

    rt_bool_t has_sample;
    rt_bool_t render_pending;
    rt_bool_t initialized;
} smoke_alarm_t;

rt_err_t smoke_alarm_init(smoke_alarm_t *ctx, const smoke_alarm_io_t *io);

/* Call from the existing 10 ms key/application scanner. */
void smoke_alarm_tick_10ms(smoke_alarm_t *ctx);

/* Call from the SW3 short-press callback. */
void smoke_alarm_on_short_press(smoke_alarm_t *ctx);

/* Call from the SW3 long-press callback. Turns the buzzer off, then mutes an alarm. */
void smoke_alarm_on_long_press(smoke_alarm_t *ctx);

float smoke_alarm_get_ppm(const smoke_alarm_t *ctx);
smoke_alarm_state_t smoke_alarm_get_state(const smoke_alarm_t *ctx);
smoke_alarm_page_t smoke_alarm_get_page(const smoke_alarm_t *ctx);
rt_bool_t smoke_alarm_is_muted(const smoke_alarm_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SMOKE_ALARM_H */

