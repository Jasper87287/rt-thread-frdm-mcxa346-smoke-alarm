# 阶段 3：自动告警逻辑接入说明

当前工作目录没有原始工程源码，因此这里提供的是可移植模块：

- `smoke_alarm.c`
- `smoke_alarm.h`

模块只依赖 RT-Thread 类型/打印接口。ADC、蜂鸣器和 OLED 通过 `smoke_alarm_io_t` 的四个回调接入，不需要修改模块内部逻辑。

## 1. 策略

| 项目 | 参数 | 行为 |
|---|---:|---|
| ADC 采样周期 | 100 ms | 每次调用 `smoke_alarm_tick_10ms()` 时计数 |
| 显示刷新周期 | 200 ms | 调用 OLED 页面刷新回调 |
| 软件低通滤波 | alpha=0.25 | 抑制烟雾 ADC 抖动 |
| 告警触发 | 浓度 > 50 | 连续 3 个有效采样后进入告警 |
| 告警解除 | 浓度 <= 40 且温度正常 | 连续 5 个有效采样后回主页 |
| 告警噪声 | 10 个浓度单位 | 防止 50 附近反复跳变 |
| SW3 短按 | 正常态 | 主页 -> 告警页 -> 故障页 -> 主页 |
| SW3 长按 | 任意状态 | 立即关闭蜂鸣器；若正在告警则进入静音 |
| 静音恢复 | 数值恢复达标 | 自动退出静音、关蜂鸣器并回主页 |

告警/静音期间短按不会离开告警页。静音只关闭蜂鸣器，不解除告警锁存；数值恢复后才自动回主页。之后再次超过 50 会重新触发蜂鸣器。

## 2. 放入工程

把 `smoke_alarm.c`、`smoke_alarm.h` 放入 `applications/`，并让 RT-Thread 的 SConscript 包含 `applications` 下的全部 C 文件。多数工程已经使用：

```python
SrcGlob = Glob('*.c')
```

如果当前 `applications/SConscript` 是显式文件列表，需要加入：

```python
src += ['smoke_alarm.c']
```

## 3. `debug_cmd.c` 中的适配代码

下面的函数名需要替换成你工程现有的实际函数名；模块不依赖这几个名字。

```c
#include "smoke_alarm.h"

static smoke_alarm_t g_smoke_alarm;

static rt_bool_t app_smoke_read_ppm(float *ppm)
{
    /* 按 hal_smoke_read_ppm 的真实返回类型修改这一行。 */
    return (hal_smoke_read_ppm(ppm) == RT_EOK) ? RT_TRUE : RT_FALSE;
}

static rt_bool_t app_temperature_over_limit(void)
{
    /* 温度传感器接入前固定返回 RT_FALSE。 */
    return RT_FALSE;
}

static void app_set_buzzer(rt_bool_t enabled)
{
    /* 根据你的蜂鸣器 HAL 包装；低电平触发由这里处理。 */
    hal_buzzer_set(enabled);
}

static void app_show_smoke_page(smoke_alarm_page_t page,
                                float ppm,
                                rt_bool_t muted)
{
    char ppm_text[16];
    int ppm_x10 = (int)((ppm * 10.0f) + 0.5f);

    rt_snprintf(ppm_text, sizeof(ppm_text), "%d.%d",
                ppm_x10 / 10, ppm_x10 % 10);

    switch (page)
    {
    case SMOKE_ALARM_PAGE_HOME:
        /* 改成你的主页刷新函数，主页显示 SMOKE: XX.X */
        oled_show_home(ppm_text, muted);
        break;

    case SMOKE_ALARM_PAGE_ALARM:
        /* 告警页显示实时浓度和 muted 状态 */
        oled_show_alarm(ppm_text, muted);
        break;

    case SMOKE_ALARM_PAGE_FAULT:
        /* 阶段 4 再接 ADC 故障原因 */
        oled_show_fault();
        break;

    default:
        break;
    }
}

static const smoke_alarm_io_t g_smoke_alarm_io =
{
    .read_ppm = app_smoke_read_ppm,
    .temperature_over_limit = app_temperature_over_limit,
    .set_buzzer = app_set_buzzer,
    .show_page = app_show_smoke_page,
};
```

在 OLED、ADC、蜂鸣器初始化完成后初始化：

```c
hal_smoke_adc_init();

if (smoke_alarm_init(&g_smoke_alarm, &g_smoke_alarm_io) != RT_EOK)
{
    rt_kprintf("[SMOKE] stage-3 init failed\r\n");
}
```

在原来的 10 ms 扫描处调用：

```c
smoke_alarm_tick_10ms(&g_smoke_alarm);
```

短按回调中，删除原来的页面循环代码，改为：

```c
smoke_alarm_on_short_press(&g_smoke_alarm);
```

长按达到 1 秒时：

```c
smoke_alarm_on_long_press(&g_smoke_alarm);
```

同时删除长按分支里重复的 `beep off` 操作，避免页面状态和蜂鸣器状态由两套逻辑控制。

## 4. 接入注意事项

1. `hal_smoke_read_ppm()` 必须返回 0.0 到 100.0 的相对浓度。模块会把负值和 NaN 当 0，把大于 100 的值截到 100。
2. `set_buzzer(RT_TRUE/RT_FALSE)` 的极性由适配函数处理；当前硬件为低电平触发，可在 `app_set_buzzer()` 内取反。
3. `show_page()` 会每 200 ms 调用一次。如果 OLED 驱动刷新较慢，可增大 `SMOKE_ALARM_DISPLAY_PERIOD_MS`。
4. 当前 `smoke_adc` 串口命令会阻塞 10 秒。测试自动告警时不要同时运行该命令，否则如果它与 10 ms 扫描处于同一线程，自动逻辑也会暂停。
5. 建议先记录洁净空气的稳定值，再根据现场波动调整 `SMOKE_ALARM_FILTER_ALPHA`、`SMOKE_ALARM_TRIGGER_SAMPLES`。触发阈值仍保持 50，解除阈值保持 40。
6. 阶段 4 接入温度传感器时，只需填充 `app_temperature_over_limit()`；温控与烟控共用同一回差/锁存状态机。
