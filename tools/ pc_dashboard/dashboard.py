#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""RT-Thread smoke/fire alarm dashboard for the FRDM-MCXA346 project."""

import csv
import queue
import threading
import time
from collections import deque
from datetime import datetime
from pathlib import Path
import tkinter as tk
from tkinter import filedialog
from tkinter import messagebox
from tkinter import scrolledtext
from tkinter import ttk

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None


APP_TITLE = "烟雾火灾报警监测系统 - 电脑端仪表盘"
BAUD_RATE = 115200
MAX_POINTS = 120
POLL_MS = 80


class DashboardApp:
    def __init__(self, root):
        self.root = root
        self.root.title(APP_TITLE)
        self.root.geometry("1180x780")
        self.root.minsize(980, 660)

        self.serial_port = None
        self.reader_thread = None
        self.reader_stop = threading.Event()
        self.rx_queue = queue.Queue()
        self.smoke_history = deque(maxlen=MAX_POINTS)
        self.temp_history = deque(maxlen=MAX_POINTS)
        self.csv_rows = []
        self.last_state = None
        self.last_packet = None
        self.port_map = {}

        self._build_style()
        self._build_ui()
        self.refresh_ports()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.after(POLL_MS, self._poll_queue)

    def _build_style(self):
        style = ttk.Style(self.root)
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        style.configure("Title.TLabel", font=("Microsoft YaHei UI", 15, "bold"))
        style.configure("Value.TLabel", font=("Consolas", 22, "bold"))
        style.configure("CardTitle.TLabel", font=("Microsoft YaHei UI", 10))
        style.configure("State.TLabel", font=("Microsoft YaHei UI", 12, "bold"))

    def _build_ui(self):
        header = ttk.Frame(self.root, padding=(12, 10, 12, 6))
        header.pack(fill="x")
        ttk.Label(header, text=APP_TITLE, style="Title.TLabel").pack(side="left")

        conn = ttk.LabelFrame(self.root, text="串口连接", padding=8)
        conn.pack(fill="x", padx=12, pady=6)

        ttk.Label(conn, text="端口:").pack(side="left")
        self.port_combo = ttk.Combobox(conn, width=34, state="readonly")
        self.port_combo.pack(side="left", padx=(6, 8))
        self.refresh_button = ttk.Button(conn, text="刷新", command=self.refresh_ports)
        self.refresh_button.pack(side="left")
        ttk.Label(conn, text="115200 8N1").pack(side="left", padx=12)
        self.connect_button = ttk.Button(conn, text="连接", command=self.toggle_connection)
        self.connect_button.pack(side="right")

        cards = ttk.Frame(self.root, padding=(12, 0))
        cards.pack(fill="x")
        self.smoke_var = tk.StringVar(value="--")
        self.temp_var = tk.StringVar(value="--")
        self.humi_var = tk.StringVar(value="--")
        self.state_var = tk.StringVar(value="--")
        self._make_card(cards, "烟雾相对浓度", self.smoke_var, "ppm", 0)
        self._make_card(cards, "温度", self.temp_var, "°C", 1)
        self._make_card(cards, "湿度", self.humi_var, "%RH", 2)
        self._make_card(cards, "系统状态", self.state_var, "", 3)

        chart_frame = ttk.LabelFrame(self.root, text="实时趋势", padding=8)
        chart_frame.pack(fill="both", expand=False, padx=12, pady=6)
        self.chart = tk.Canvas(chart_frame, height=245, bg="#101820", highlightthickness=0)
        self.chart.pack(fill="both", expand=True)
        self.chart.bind("<Configure>", lambda _event: self._draw_chart())

        status_frame = ttk.Frame(self.root, padding=(12, 0))
        status_frame.pack(fill="x")
        self.alarm_label = self._make_indicator(status_frame, "报警", 0)
        self.buzzer_label = self._make_indicator(status_frame, "蜂鸣器", 1)
        self.relay_label = self._make_indicator(status_frame, "继电器", 2)
        self.dht_label = self._make_indicator(status_frame, "DHT 数据", 3)

        command_frame = ttk.LabelFrame(self.root, text="远程控制", padding=8)
        command_frame.pack(fill="x", padx=12, pady=6)
        commands = [
            ("烟雾 60", "cmd_smoke_sim 60"),
            ("烟雾恢复 30", "cmd_smoke_sim 30"),
            ("关闭烟雾模拟", "cmd_smoke_sim off"),
            ("温度 36", "cmd_temp_sim 36"),
            ("温度恢复 30", "cmd_temp_sim 30"),
            ("关闭温度模拟", "cmd_temp_sim off"),
            ("跳过预热", "cmd_warmup_skip"),
            ("DHT 测试", "cmd_dht22_test"),
            ("继电器 ON", "cmd_relay_test on"),
            ("继电器 OFF", "cmd_relay_test off"),
        ]
        for index, (text, command) in enumerate(commands):
            button = ttk.Button(command_frame, text=text, command=lambda cmd=command: self.send_command(cmd))
            button.grid(row=index // 5, column=index % 5, sticky="ew", padx=4, pady=4)
        for column in range(5):
            command_frame.columnconfigure(column, weight=1)

        log_frame = ttk.LabelFrame(self.root, text="日志", padding=6)
        log_frame.pack(fill="both", expand=True, padx=12, pady=(4, 10))
        self.log_widget = scrolledtext.ScrolledText(log_frame, height=9, wrap="word", state="disabled")
        self.log_widget.pack(fill="both", expand=True, side="left")
        side = ttk.Frame(log_frame)
        side.pack(side="right", fill="y", padx=(6, 0))
        ttk.Button(side, text="清空日志", command=self.clear_log).pack(fill="x", pady=2)
        ttk.Button(side, text="保存 CSV", command=self.save_csv).pack(fill="x", pady=2)
        ttk.Button(side, text="发送 telemetry", command=lambda: self.send_command("cmd_telemetry on")).pack(fill="x", pady=2)

        self.status_var = tk.StringVar(value="未连接")
        ttk.Label(self.root, textvariable=self.status_var, anchor="w", padding=(12, 2)).pack(fill="x")

    def _make_card(self, parent, title, variable, unit, column):
        frame = ttk.LabelFrame(parent, text=title, padding=8)
        frame.grid(row=0, column=column, sticky="nsew", padx=4, pady=4)
        parent.columnconfigure(column, weight=1)
        ttk.Label(frame, textvariable=variable, style="Value.TLabel").pack(anchor="center")
        if unit:
            ttk.Label(frame, text=unit).pack(anchor="center")

    def _make_indicator(self, parent, title, column):
        frame = ttk.LabelFrame(parent, text=title, padding=8)
        frame.grid(row=0, column=column, sticky="nsew", padx=4, pady=4)
        parent.columnconfigure(column, weight=1)
        label = ttk.Label(frame, text="--", style="State.TLabel", anchor="center")
        label.pack(fill="x")
        return label

    def refresh_ports(self):
        if serial is None or list_ports is None:
            self.status_var.set("缺少 pyserial，请执行: pip install pyserial")
            return
        ports = list(list_ports.comports())
        self.port_map.clear()
        values = []
        for port in ports:
            text = f"{port.device} - {port.description}"
            self.port_map[text] = port.device
            values.append(text)
        self.port_combo["values"] = values
        if values and self.port_combo.get() not in values:
            self.port_combo.set(values[0])
        self.status_var.set(f"发现 {len(values)} 个串口")

    def toggle_connection(self):
        if self.serial_port is not None and self.serial_port.is_open:
            self.disconnect_serial()
        else:
            self.connect_serial()

    def connect_serial(self):
        if serial is None:
            messagebox.showerror("缺少依赖", "请先安装 pyserial：\npip install pyserial")
            return
        selection = self.port_combo.get()
        port_name = self.port_map.get(selection, selection.split(" - ")[0].strip())
        if not port_name:
            messagebox.showwarning("没有端口", "请选择串口")
            return
        try:
            self.serial_port = serial.Serial(port_name, BAUD_RATE, timeout=0.2)
            self.reader_stop.clear()
            self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
            self.reader_thread.start()
            self.connect_button.config(text="断开")
            self.port_combo.config(state="disabled")
            self.status_var.set(f"已连接 {port_name} @ {BAUD_RATE}")
            self._log(f"已连接 {port_name}")
            self.root.after(200, lambda: self.send_command("cmd_telemetry on"))
        except Exception as exc:
            self.serial_port = None
            messagebox.showerror("串口错误", str(exc))
            self.status_var.set("连接失败")

    def disconnect_serial(self):
        if self.serial_port is not None:
            try:
                self.send_command("cmd_telemetry off", show_log=False)
            except Exception:
                pass
        self.reader_stop.set()
        if self.serial_port is not None:
            try:
                self.serial_port.close()
            except Exception:
                pass
        self.serial_port = None
        self.reader_thread = None
        self.connect_button.config(text="连接")
        self.port_combo.config(state="readonly")
        self.status_var.set("已断开")
        self._log("已断开连接")

    def _reader_loop(self):
        buffer = bytearray()
        while not self.reader_stop.is_set():
            try:
                chunk = self.serial_port.read(256)
                if not chunk:
                    continue
                buffer.extend(chunk)
                while b"\n" in buffer:
                    raw, _, rest = buffer.partition(b"\n")
                    buffer = bytearray(rest)
                    line = raw.rstrip(b"\r").decode("utf-8", errors="replace").strip()
                    self.rx_queue.put(("line", line))
            except Exception as exc:
                if not self.reader_stop.is_set():
                    self.rx_queue.put(("error", str(exc)))
                break

    def _poll_queue(self):
        while True:
            try:
                kind, payload = self.rx_queue.get_nowait()
            except queue.Empty:
                break
            if kind == "line":
                self._handle_line(payload)
            elif kind == "error":
                self._log(f"串口错误: {payload}")
                self.disconnect_serial()
        self.root.after(POLL_MS, self._poll_queue)

    def _handle_line(self, line):
        if not line:
            return
        if line.startswith("$DATA,"):
            self._parse_data(line)
        else:
            self._log(line)

    def _parse_data(self, line):
        fields = line.split(",")
        if len(fields) < 10:
            self._log(f"数据格式错误: {line}")
            return
        try:
            tick = int(fields[1])
            smoke = int(fields[2]) / 10.0
            temperature = int(fields[3]) / 10.0
            humidity = int(fields[4]) / 10.0
            state = fields[5]
            alarm = int(fields[6])
            relay = int(fields[7])
            buzzer = int(fields[8])
            dht_valid = int(fields[9])
        except ValueError:
            self._log(f"数据解析失败: {line}")
            return

        self.smoke_var.set(f"{smoke:.1f}")
        if dht_valid:
            self.temp_var.set(f"{temperature:.1f}")
            self.humi_var.set(f"{humidity:.1f}")
        else:
            self.temp_var.set("--")
            self.humi_var.set("--")
        self.state_var.set(state)

        self.smoke_history.append(smoke)
        self.temp_history.append(temperature if dht_valid else None)

        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.last_packet = (now, tick, smoke, temperature, humidity, state, alarm, relay, buzzer, dht_valid)
        self.csv_rows.append(self.last_packet)

        self._set_indicator(self.alarm_label, bool(alarm), "ALARM", "OFF")
        self._set_indicator(self.buzzer_label, bool(buzzer), "ON", "OFF")
        self._set_indicator(self.relay_label, bool(relay), "ON", "OFF")
        self._set_indicator(self.dht_label, bool(dht_valid), "OK", "NO DATA")

        if state != self.last_state:
            self._log(f"状态变化: {self.last_state or '--'} -> {state}")
            self.last_state = state

        self._draw_chart()
        self.status_var.set(f"最后更新: {now}  设备时间: {tick} ms")

    def _set_indicator(self, label, active, active_text, inactive_text):
        label.config(text=active_text if active else inactive_text)
        style_name = "Active.TLabel" if active else "Inactive.TLabel"
        style = ttk.Style(self.root)
        style.configure("Active.TLabel", foreground="#1a7f37", font=("Microsoft YaHei UI", 12, "bold"))
        style.configure("Inactive.TLabel", foreground="#777777", font=("Microsoft YaHei UI", 12, "bold"))
        label.config(style=style_name)

    def _draw_chart(self):
        canvas = self.chart
        canvas.delete("all")
        width = max(canvas.winfo_width(), 400)
        height = max(canvas.winfo_height(), 160)
        left, right, top, bottom = 42, 18, 18, 26
        plot_w = width - left - right
        plot_h = height - top - bottom

        for index in range(5):
            y = top + int(plot_h * index / 4)
            canvas.create_line(left, y, left + plot_w, y, fill="#29323d", width=1)

        canvas.create_text(left, 8, text="烟雾 0-100", fill="#53d769", anchor="w")
        canvas.create_text(left + 115, 8, text="温度 0-60°C", fill="#ff6b6b", anchor="w")

        self._draw_series(self.smoke_history, 0.0, 100.0, "#53d769", left, top, plot_w, plot_h)
        self._draw_series(self.temp_history, 0.0, 60.0, "#ff6b6b", left, top, plot_w, plot_h)

        canvas.create_text(left, height - 12, text="最近 120 个遥测点", fill="#aab4c0", anchor="w")

    def _draw_series(self, values, vmin, vmax, color, left, top, plot_w, plot_h):
        points = list(values)
        if len(points) < 2:
            return
        previous = None
        denominator = max(len(points) - 1, 1)
        for index, value in enumerate(points):
            if value is None:
                previous = None
                continue
            normalized = max(0.0, min(1.0, (value - vmin) / (vmax - vmin)))
            x = left + int(plot_w * index / denominator)
            y = top + int(plot_h * (1.0 - normalized))
            if previous is not None:
                self.chart.create_line(previous[0], previous[1], x, y, fill=color, width=2)
            previous = (x, y)

    def send_command(self, command, show_log=True):
        if self.serial_port is None or not self.serial_port.is_open:
            if show_log:
                messagebox.showwarning("串口未连接", "请先连接串口")
            return
        try:
            self.serial_port.write((command + "\r\n").encode("ascii"))
            if show_log:
                self._log(f"> {command}")
        except Exception as exc:
            if show_log:
                self._log(f"发送失败: {exc}")

    def _log(self, message):
        timestamp = time.strftime("%H:%M:%S")
        self.log_widget.config(state="normal")
        self.log_widget.insert("end", f"[{timestamp}] {message}\n")
        self.log_widget.see("end")
        self.log_widget.config(state="disabled")

    def clear_log(self):
        self.log_widget.config(state="normal")
        self.log_widget.delete("1.0", "end")
        self.log_widget.config(state="disabled")

    def save_csv(self):
        if not self.csv_rows:
            messagebox.showinfo("没有数据", "当前还没有收到遥测数据")
            return
        filename = filedialog.asksaveasfilename(
            title="保存遥测数据",
            defaultextension=".csv",
            filetypes=[("CSV 文件", "*.csv"), ("所有文件", "*.*")],
            initialfile="smoke_alarm_data.csv",
        )
        if not filename:
            return
        headers = ["时间", "设备时间ms", "烟雾", "温度", "湿度", "状态", "报警", "继电器", "蜂鸣器", "DHT有效"]
        with open(filename, "w", newline="", encoding="utf-8-sig") as handle:
            writer = csv.writer(handle)
            writer.writerow(headers)
            writer.writerows(self.csv_rows)
        messagebox.showinfo("保存成功", f"已保存:\n{filename}")

    def on_close(self):
        self.disconnect_serial()
        self.root.destroy()


def main():
    root = tk.Tk()
    DashboardApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
