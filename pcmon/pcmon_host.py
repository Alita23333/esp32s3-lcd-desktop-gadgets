#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pcmon_host.py — GPU 性能采集 + UDP 广播（配合 ESP32-S3-LCD-1.47B 副屏）

采集 pynvml(NVIDIA GPU) 的 占用率/功耗/温度/显存，每 SEND_INTERVAL 秒
通过 UDP 发送一包 CSV 文本：

    PCMON,<seq>,<util%>,<powerW*10>,<tempC>,<vramMB>,<vramTotalMB>,<powerLimitW*10>

依赖: pip install nvidia-ml-py
"""

import socket
import sys
import time

# ===== 配置 =====
SEND_INTERVAL = 0.5      # 发送间隔（秒）
BROADCAST = True         # True: 局域网广播(255.255.255.255, 免配置)
                         # False: 单播到 TARGET_IP（广播不通/AP隔离时使用，
                         #        TARGET_IP 填副屏 IP，见屏幕底部或串口输出）
TARGET_IP = "192.168.1.100"
PORT = 9999              # 与固件 Config.h 的 UDP_PORT 保持一致
GPU_INDEX = 0            # 多显卡时选择第几块
# ================

import pynvml


def gpu_sample(h):
    """返回 (util%, powerW, tempC, vramMB, vramTotalMB, powerLimitW)"""
    u = pynvml.nvmlDeviceGetUtilizationRates(h)
    try:
        pw = pynvml.nvmlDeviceGetPowerUsage(h) / 1000.0   # mW -> W
    except pynvml.NVMLError:
        pw = 0.0                                          # 部分机型不支持
    t = pynvml.nvmlDeviceGetTemperature(h, pynvml.NVML_TEMPERATURE_GPU)
    m = pynvml.nvmlDeviceGetMemoryInfo(h)
    try:
        pl = pynvml.nvmlDeviceGetPowerManagementLimit(h) / 1000.0
    except pynvml.NVMLError:
        pl = 0.0
    return u.gpu, pw, t, m.used // 1048576, m.total // 1048576, pl


def main():
    test_mode = "--test" in sys.argv
    if test_mode:
        target, interval, n = "127.0.0.1", 0.2, 15
        print("[test] 回环模式 -> 127.0.0.1:%d" % PORT)
    elif BROADCAST:
        target, interval, n = "255.255.255.255", SEND_INTERVAL, None
    else:
        target, interval, n = TARGET_IP, SEND_INTERVAL, None

    pynvml.nvmlInit()
    h = pynvml.nvmlDeviceGetHandleByIndex(GPU_INDEX)
    name = pynvml.nvmlDeviceGetName(h)
    if isinstance(name, bytes):
        name = name.decode()
    print("GPU: %s  ->  udp://%s:%d  每 %.1fs" % (name, target, PORT, interval))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    seq = 0
    try:
        while True:
            util, pw, temp, vram, vram_total, plim = gpu_sample(h)
            msg = "PCMON,%d,%d,%d,%d,%d,%d,%d" % (
                seq & 0xFFFF,
                util,
                int(pw * 10),        # W*10 整数传输
                temp,
                vram,
                vram_total,
                int(plim * 10),
            )
            sock.sendto(msg.encode("ascii"), (target, PORT))

            if seq % 4 == 0 or test_mode:
                print("%s  GPU %3d%%  %5.1fW/%4.0fW  %3dC  VRAM %d/%dMB"
                      % (time.strftime("%H:%M:%S"), util, pw, plim, temp, vram, vram_total))
            seq += 1
            if n and seq >= n:
                break
            time.sleep(interval)
    except KeyboardInterrupt:
        pass
    finally:
        pynvml.nvmlShutdown()
        print("已退出")


if __name__ == "__main__":
    main()
