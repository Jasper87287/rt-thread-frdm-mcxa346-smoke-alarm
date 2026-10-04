#include "smoke_alarm.h"

#define SMOKE_ALARM_TICKS_FROM_MS(ms)  (((ms) + 9U) / 10U)
#define SMOKE_ALARM_SAMPLE_TICKS       SMOKE_ALARM_TICKS_FROM_MS(SMOKE_ALARM_SAMPLE_PERIOD_MS)
#define SMOKE_ALARM_DISPLAY_TICKS      SMOKE_ALARM_TICKS_FROM_MS(SMOKE_ALARM_DISPLAY_PERIOD_MS)


static const char *smoke_alarm_page_name(smoke_alarm_page_t page)
{
    switch (page)
    {
    case SMOKE_ALARM_PAGE_HOME:
        return "HOME";
    case SMOKE_ALARM_PAGE_ALARM:
        return "ALARM";
    case SMOKE_ALARM_PAGE_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

static int smoke_alarm_ppm_x10(float ppm)
{
    return (int)((ppm * 10.0f) + 0.5f);
}

static void smoke_alarm_print_ppm(const char *prefix, float ppm)
{
    int value_x10 = smoke_alarm_ppm_x10(ppm);

    rt_kprintf("[SMOKE] %s ppm=%d.%d\r\n",
               prefix, value_x10 / 10, value_x10 % 10);
}

static void smoke_alarm_render(smoke_alarm_t *ctx, rt_bool_t force)
{
    if ((ctx == RT_NULL) || (ctx->io == RT_NULL) ||
        (ctx->io->show_page == RT_NULL))
    {
        return;
    }

    if ((force == RT_TRUE) || (ctx->render_pending == RT_TRUE) ||
        (ctx->display_ticks >= SMOKE_ALARM_DISPLAY_TICKS))
    {
        ctx->io->show_page(ctx->page, ctx->ppm_filtered,
                           (ctx->state == SMOKE_ALARM_STATE_MUTED) ? RT_TRUE : RT_FALSE);
        ctx->display_ticks = 0U;
        ctx->render_pending = RT_FALSE;
    }
}

static void smoke_alarm_output(smoke_alarm_t *ctx, rt_bool_t active)
{
    if ((ctx != RT_NULL) && (ctx->io != RT_NULL) &&
        (ctx->io->set_alarm_output != RT_NULL))
    {
        ctx->io->set_alarm_output(active);
    }
}
static void smoke_alarm_beep(smoke_alarm_t *ctx, rt_bool_t enabled)
{
    if ((ctx != RT_NULL) && (ctx->io != RT_NULL) &&
        (ctx->io->set_buzzer != RT_NULL))
    {
        ctx->io->set_buzzer(enabled);
    }
}

static void smoke_alarm_enter_active(smoke_alarm_t *ctx)
{
    if (ctx->state == SMOKE_ALARM_STATE_ACTIVE)
    {
        return;
    }

    ctx->state = SMOKE_ALARM_STATE_ACTIVE;
    ctx->page = SMOKE_ALARM_PAGE_ALARM;
    ctx->render_pending = RT_TRUE;
    smoke_alarm_output(ctx, RT_TRUE);
    smoke_alarm_beep(ctx, RT_TRUE);
    smoke_alarm_print_ppm("ALARM", ctx->ppm_filtered);
}

static void smoke_alarm_return_home(smoke_alarm_t *ctx)
{
    ctx->state = SMOKE_ALARM_STATE_NORMAL;
    ctx->page = SMOKE_ALARM_PAGE_HOME;
    ctx->trigger_count = 0U;
    ctx->release_count = 0U;
    ctx->render_pending = RT_TRUE;
    smoke_alarm_output(ctx, RT_FALSE);
    smoke_alarm_beep(ctx, RT_FALSE);
    smoke_alarm_print_ppm("RECOVERED", ctx->ppm_filtered);
}

static void smoke_alarm_process_sample(smoke_alarm_t *ctx)
{
    float sample;
    rt_bool_t over_limit;
    rt_bool_t smoke_high;
    rt_bool_t smoke_release;
    rt_bool_t temperature_high = RT_FALSE;

    if ((ctx->io == RT_NULL) || (ctx->io->read_ppm == RT_NULL) ||
        (ctx->io->read_ppm(&sample) != RT_TRUE))
    {
        if (ctx->invalid_count < 255U)
        {
            ctx->invalid_count++;
        }

        if ((ctx->invalid_count == 5U) &&
            (ctx->state == SMOKE_ALARM_STATE_NORMAL))
        {
            rt_kprintf("[SMOKE] ADC read failed; keep last valid value\r\n");
        }

        return;
    }

    ctx->invalid_count = 0U;

    /* NaN fails this comparison and is mapped to zero. */
    if ((sample != sample) || (sample < 0.0f))
    {
        sample = 0.0f;
    }
    else if (sample > 100.0f)
    {
        sample = 100.0f;
    }

    ctx->ppm_raw_last = sample;

    if (ctx->has_sample == RT_TRUE)
    {
        ctx->ppm_filtered += (sample - ctx->ppm_filtered) * SMOKE_ALARM_FILTER_ALPHA;
    }
    else
    {
        ctx->ppm_filtered = sample;
        ctx->has_sample = RT_TRUE;
    }

    if ((ctx->io->temperature_over_limit != RT_NULL) &&
        (ctx->io->temperature_over_limit() == RT_TRUE))
    {
        temperature_high = RT_TRUE;
    }

    smoke_high = (ctx->ppm_filtered > SMOKE_ALARM_PPM_TRIGGER) ? RT_TRUE : RT_FALSE;
    over_limit = (smoke_high == RT_TRUE) || (temperature_high == RT_TRUE);

    smoke_release = ((ctx->ppm_filtered <= SMOKE_ALARM_PPM_RELEASE) &&
                     (temperature_high == RT_FALSE)) ? RT_TRUE : RT_FALSE;

    if (ctx->warmup_ticks > 0U)
    {
        ctx->trigger_count = 0U;
        ctx->release_count = 0U;
        return;
    }
    switch (ctx->state)
    {
    case SMOKE_ALARM_STATE_NORMAL:
        if (over_limit == RT_TRUE)
        {
            ctx->release_count = 0U;
            if (ctx->trigger_count < 255U)
            {
                ctx->trigger_count++;
            }

            if (ctx->trigger_count >= SMOKE_ALARM_TRIGGER_SAMPLES)
            {
                smoke_alarm_enter_active(ctx);
            }
        }
        else
        {
            ctx->trigger_count = 0U;
            ctx->release_count = 0U;
        }
        break;

    case SMOKE_ALARM_STATE_ACTIVE:
    case SMOKE_ALARM_STATE_MUTED:
        if (smoke_release == RT_TRUE)
        {
            ctx->trigger_count = 0U;
            if (ctx->release_count < 255U)
            {
                ctx->release_count++;
            }

            if (ctx->release_count >= SMOKE_ALARM_RELEASE_SAMPLES)
            {
                smoke_alarm_return_home(ctx);
            }
        }
        else
        {
            ctx->release_count = 0U;
        }
        break;

    case SMOKE_ALARM_STATE_FAULT:
    default:
        /* Fault recovery is reserved for stage 4. */
        break;
    }
}

rt_err_t smoke_alarm_init(smoke_alarm_t *ctx, const smoke_alarm_io_t *io)
{
    if ((ctx == RT_NULL) || (io == RT_NULL) || (io->read_ppm == RT_NULL) ||
        (io->set_buzzer == RT_NULL) || (io->show_page == RT_NULL))
    {
        return -RT_EINVAL;
    }

    rt_memset(ctx, 0, sizeof(*ctx));
    ctx->io = io;
    ctx->state = SMOKE_ALARM_STATE_NORMAL;
    ctx->page = SMOKE_ALARM_PAGE_HOME;
    ctx->render_pending = RT_TRUE;
    ctx->initialized = RT_TRUE;
    ctx->sample_ticks = SMOKE_ALARM_SAMPLE_TICKS;
    ctx->warmup_ticks = SMOKE_ALARM_WARMUP_MS / 10U;

    smoke_alarm_beep(ctx, RT_FALSE);
    smoke_alarm_output(ctx, RT_FALSE);
    rt_kprintf("[SMOKE] warmup=%u seconds\r\n", SMOKE_ALARM_WARMUP_MS / 1000U);
    smoke_alarm_render(ctx, RT_TRUE);

    rt_kprintf("[SMOKE] stage-3 alarm ready: trigger>%d.%d release<=%d.%d\r\n",
               (int)SMOKE_ALARM_PPM_TRIGGER,
               (int)((SMOKE_ALARM_PPM_TRIGGER - (float)(int)SMOKE_ALARM_PPM_TRIGGER) * 10.0f + 0.5f),
               (int)SMOKE_ALARM_PPM_RELEASE,
               (int)((SMOKE_ALARM_PPM_RELEASE - (float)(int)SMOKE_ALARM_PPM_RELEASE) * 10.0f + 0.5f));

    return RT_EOK;
}

void smoke_alarm_tick_10ms(smoke_alarm_t *ctx)
{
    if ((ctx == RT_NULL) || (ctx->initialized != RT_TRUE))
    {
        return;
    }

    if (ctx->warmup_ticks > 0U)
    {
        ctx->warmup_ticks--;
        if (ctx->warmup_ticks == 0U)
        {
            rt_kprintf("[SMOKE] warmup complete\r\n");
        }
    }

    if (ctx->sample_ticks < 65535U)
    {
        ctx->sample_ticks++;
    }

    if (ctx->sample_ticks >= SMOKE_ALARM_SAMPLE_TICKS)
    {
        ctx->sample_ticks = 0U;
        smoke_alarm_process_sample(ctx);
    }

    if (ctx->display_ticks < 65535U)
    {
        ctx->display_ticks++;
    }

    smoke_alarm_render(ctx, RT_FALSE);
}

void smoke_alarm_on_short_press(smoke_alarm_t *ctx)
{
    if ((ctx == RT_NULL) || (ctx->initialized != RT_TRUE))
    {
        return;
    }


    if ((ctx->state == SMOKE_ALARM_STATE_ACTIVE) ||
        (ctx->state == SMOKE_ALARM_STATE_MUTED))
    {
        ctx->render_pending = RT_TRUE;
        rt_kprintf("[SMOKE] active alarm keeps page=%s\r\n",
                   smoke_alarm_page_name(ctx->page));
        return;
    }

    switch (ctx->page)
    {
    case SMOKE_ALARM_PAGE_HOME:
        ctx->page = SMOKE_ALARM_PAGE_ALARM;
        break;
    case SMOKE_ALARM_PAGE_ALARM:
        ctx->page = SMOKE_ALARM_PAGE_FAULT;
        break;
    case SMOKE_ALARM_PAGE_FAULT:
    default:
        ctx->page = SMOKE_ALARM_PAGE_HOME;
        break;
    }

    ctx->render_pending = RT_TRUE;
    rt_kprintf("[SMOKE] page=%s\r\n", smoke_alarm_page_name(ctx->page));
}

void smoke_alarm_on_long_press(smoke_alarm_t *ctx)
{
    if ((ctx == RT_NULL) || (ctx->initialized != RT_TRUE))
    {
        return;
    }


    smoke_alarm_beep(ctx, RT_FALSE);

    if (ctx->state == SMOKE_ALARM_STATE_ACTIVE)
    {
        ctx->state = SMOKE_ALARM_STATE_MUTED;
        ctx->page = SMOKE_ALARM_PAGE_ALARM;
        ctx->render_pending = RT_TRUE;
        smoke_alarm_print_ppm("MUTED", ctx->ppm_filtered);
    }

}

float smoke_alarm_get_ppm(const smoke_alarm_t *ctx)
{
    return (ctx == RT_NULL) ? 0.0f : ctx->ppm_filtered;
}

smoke_alarm_state_t smoke_alarm_get_state(const smoke_alarm_t *ctx)
{
    return (ctx == RT_NULL) ? SMOKE_ALARM_STATE_FAULT : ctx->state;
}

smoke_alarm_page_t smoke_alarm_get_page(const smoke_alarm_t *ctx)
{
    return (ctx == RT_NULL) ? SMOKE_ALARM_PAGE_FAULT : ctx->page;
}

rt_bool_t smoke_alarm_is_muted(const smoke_alarm_t *ctx)
{
    return ((ctx != RT_NULL) &&
            (ctx->state == SMOKE_ALARM_STATE_MUTED)) ? RT_TRUE : RT_FALSE;
}


