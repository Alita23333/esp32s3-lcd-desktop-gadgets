# -*- coding: utf-8 -*-
"""WiFi 雷达自动测试：遥测列表、重扫、稳定性"""
import serial, time, sys

PORT = 'COM5'
s = serial.Serial(PORT, 115200, timeout=0.1)
s.dtr = False; s.rts = False
print('等待启动 + 首轮扫描...', flush=True)
time.sleep(4.0)
s.reset_input_buffer()

line_buf = b''
nets = {}

def pump(sec, quiet=True):
    global line_buf
    t0 = time.time()
    while time.time() - t0 < sec:
        c = s.read(512)
        if not c: continue
        line_buf += c
        while b'\n' in line_buf:
            line, line_buf = line_buf.split(b'\n', 1)
            txt = line.decode('utf-8', errors='replace').strip()
            if txt.startswith('#W,') and not txt.startswith('#WEND'):
                # #W,序号,SSID(可含逗号),RSSI,信道,加密 → 负索引取尾部数值字段
                p = txt[3:].split(',')
                if len(p) >= 5:
                    ssid = ','.join(p[1:-3])
                    nets[ssid] = int(p[-3])
            elif txt and not quiet:
                print('  |', txt, flush=True)

def get_nets():
    nets.clear()
    s.write(b'p')
    pump(1.0)
    return dict(nets)

print('== 1. 遥测：网络列表 ==', flush=True)
t = get_nets()
if len(t) == 0:   # 重试一次（可能错过首轮）
    print('   首次为空，重试...', flush=True)
    t = get_nets()
print(f'   发现 {len(t)} 个 SSID: {list(t)[:6]}', flush=True)
assert len(t) >= 1, '未发现任何网络'
assert '1145' in t, f'列表中无 1145: {t}'
print(f"   PASS: 1145 在列表中, RSSI={t['1145']} dBm", flush=True)
assert -100 < t['1145'] < 0, 'RSSI 异常'

print('== 2. 强制重扫（回显模式） ==', flush=True)
s.write(b'r')
pump(3.0, quiet=False)
t2 = get_nets()
print(f'   重扫后 {len(t2)} 个 SSID', flush=True)
assert len(t2) >= 1, '重扫后无网络'
print('   PASS: 重扫正常', flush=True)

print('== 3. 稳定性观察 15 秒 ==', flush=True)
t0 = time.time()
while time.time() - t0 < 15:
    c = s.read(512)
    if c:
        txt = c.decode('utf-8', errors='replace')
        if 'boot:' in txt or 'Guru' in txt or 'assert' in txt:
            print('   FAIL: 检测到异常重启', flush=True); sys.exit(1)
print('   PASS: 15 秒无崩溃重启', flush=True)

print('== ALL TESTS PASSED ==', flush=True)
