# 基于 RT-Thread 的烟雾火灾报警监测系统

## 项目简介

本项目基于 NXP FRDM-MCXA346 开发板和 RT-Thread 5.2.2，
实现烟雾浓度、温度、湿度采集，OLED 显示，
蜂鸣器报警、继电器联动和电脑端 Python 仪表盘。

## 主要功能

- MQ-2 烟雾浓度采集
- DHT22 温湿度采集
- OLED 主页、告警页、故障页
- 烟雾超过 50 自动告警
- 温度超过 35°C 自动告警
- 长按 SW3 消音
- 蜂鸣器和继电器联动
- 数值恢复后自动解除告警
- MSH 调试命令
- Python 电脑端仪表盘

## 硬件平台

- 主控：NXP MCXA346
- 开发板：FRDM-MCXA346
- 操作系统：RT-Thread 5.2.2
- 开发工具：Keil MDK-ARM

## 接线说明

| 模块 | 引脚 | 说明 |
|---|---|---|
| MQ-2 AO | P1_14 | ADC1_CH12 |
| MQ-2 VCC | 5V | 独立 5V 供电 |
| MQ-2 GND | GND | 共地 |
| DHT22 OUT | P3_7 | 单总线数据 |
| DHT22 VCC | 3.3V | 3.3V 供电 |
| DHT22 GND | GND | 共地 |
| OLED SCL | P2_5 | 软件 I2C |
| OLED SDA | P2_7 | 软件 I2C |
| SW3 | P0_6 | 低电平有效 |
| 蜂鸣器 | P2_20 | 低电平触发 |
| 继电器 IN | P2_1 | 低电平触发 |

## 软件框架

- HAL 层：GPIO、ADC、DHT22、OLED、蜂鸣器、继电器
- 业务层：采样、滤波、阈值、告警状态机
- 显示层：OLED 页面
- 调试层：MSH 命令、`$DATA` 遥测、Python 仪表盘

## 告警策略

烟雾：

- 浓度 > 50，连续 3 次采样触发
- 浓度 <= 40，连续 5 次采样恢复

温度：

- 温度 >= 35°C 触发
- 温度 <= 33°C 恢复

长按 SW3：

- 关闭蜂鸣器
- 继电器保持吸合
- 状态切换为 MUTED

## MSH 命令

```text
smoke_adc
cmd_dht22_test
cmd_dht22_pin_test
cmd_smoke_sim
cmd_temp_sim
cmd_relay_test
cmd_warmup_skip
cmd_telemetry
编译方法
1. 使用 Keil MDK 打开 project.uvprojx。
2. 编译生成 rtthread.axf。
3. 下载到 FRDM-MCXA346。
4. 连接 OLED、MQ-2、DHT22、蜂鸣器和继电器。
Python 电脑端仪表盘
cd tools/pc_dashboard
python -m pip install -r requirements.txt
python dashboard.py
软件支持：
- 串口连接
- 实时数值
- 烟雾/温度曲线
- 报警、蜂鸣器、继电器状态
- CSV 数据保存
- 远程测试命令
串口遥测协议
$DATA,tick,smoke,temp,humi,state,alarm,relay,buzzer,dht_valid
示例：
$DATA,123456,97,253,582,NORMAL,0,0,0,1
演示视频
B 站链接：
(https://www.bilibili.com/video/BV1rfH666Evc/?share_source=copy_web&vd_source=b3051c4c718899391d78e43a0ac56642)
RT-Thread 论坛文章
(https://club.rt-thread.org/ask/article/0e815ddfc029ea9c.html)
