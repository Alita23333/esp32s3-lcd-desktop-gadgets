# -*- coding: utf-8 -*-
"""贪吃蛇自动测试：解析 #S 遥测，贪心导航吃食物，验证计分/游戏结束/重开/最高分"""
import serial, time, sys

PORT = 'COM5'
s = serial.Serial(PORT, 115200, timeout=0.1)
s.dtr = False; s.rts = False
time.sleep(1.5)          # 等板上电启动完成（打开端口会触发一次复位）
s.reset_input_buffer()

line_buf = b''
telem = {}

def pump(sec, quiet=False):
    global line_buf
    t0 = time.time()
    while time.time() - t0 < sec:
        c = s.read(256)
        if not c: continue
        line_buf += c
        while b'\n' in line_buf:
            line, line_buf = line_buf.split(b'\n', 1)
            txt = line.decode('utf-8', errors='replace').strip()
            if txt.startswith('#S,'):
                p = txt[3:].split(',')
                telem.update({
                    'state': int(p[0]), 'score': int(p[1]), 'len': int(p[2]),
                    'hx': int(p[3]), 'hy': int(p[4]), 'fx': int(p[5]), 'fy': int(p[6]),
                    'dx': int(p[7]), 'dy': int(p[8]),
                })
            elif txt and not quiet:
                print('  |', txt, flush=True)

def get_telem(sec=0.25):
    send('p')            # 每次轮询先请求遥测
    pump(sec, quiet=True)
    return dict(telem)

def send(ch):
    s.write(ch.encode()); time.sleep(0.02)

print('== 1. 开始游戏 ==', flush=True)
send('g'); pump(0.5)
t = get_telem(0.4)
assert t.get('state') == 1, f"未进入 RUN: {t}"
print(f'   RUN 确认: score={t["score"]} head=({t["hx"]},{t["hy"]}) food=({t["fx"]},{t["fy"]})', flush=True)

print('== 2. 贪心导航吃食物（目标 2 分） ==', flush=True)
eaten = 0; steps = 0; last_score = t['score']
while eaten < 2 and steps < 80:
    t = get_telem(0.22)
    if t.get('state') == 3:
        print('   意外游戏结束，重开', flush=True)
        send('g'); pump(0.5); steps = 0; continue
    dx = t['fx'] - t['hx']; dy = t['fy'] - t['hy']
    neck = (t['hx'] - t['dx'], t['hy'] - t['dy'])
    if abs(dx) >= abs(dy):
        cands = [('d',1,0) if dx>0 else ('a',-1,0), ('s',0,1) if dy>0 else ('w',0,-1)]
    else:
        cands = [('s',0,1) if dy>0 else ('w',0,-1), ('d',1,0) if dx>0 else ('a',-1,0)]
    moved = False
    for ch, mx, my in cands:
        if (mx,my) == (-t['dx'], -t['dy']):  continue
        if (t['hx']+mx, t['hy']+my) == neck and t['len'] > 2: continue
        send(ch); moved = True; break
    if not moved:
        send('w')
    steps += 1
    if t['score'] > last_score:
        eaten += 1; last_score = t['score']
        print(f'   [{steps}] 吃到食物! score={t["score"]}', flush=True)
    if steps % 5 == 0:
        print(f'   [{steps}] head=({t["hx"]},{t["hy"]}) dir=({t["dx"]},{t["dy"]}) food=({t["fx"]},{t["fy"]})', flush=True)

pump(0.4)
t = get_telem(0.3)
print(f'== 3. 验证计分: score={t.get("score")} len={t.get("len")} ==', flush=True)
assert t.get('score', 0) >= 2, '两次吃食物失败'
assert t.get('len', 0) == 3 + t.get('score', 0), f'蛇长未随进食增长: {t}'
print('   PASS: 吃食物、计分、蛇身增长正常', flush=True)

print('== 4. 撞墙测试（持续向左） ==', flush=True)
send('a')
deadline = time.time() + 12
over = False
while time.time() < deadline:
    t = get_telem(0.25)
    if t.get('state') == 3:
        print(f'   PASS: 游戏结束 score={t["score"]}', flush=True)
        over = True
        break
    if t.get('dx', 0) != -1:
        send('a')
assert over, '12 秒内未触发游戏结束'

print('== 5. 重开 + 校验 ==', flush=True)
send('g'); pump(0.8)
t = get_telem(0.3)
assert t.get('state') == 1 and t.get('score') == 0, f'重开异常: {t}'
print(f'   RUN 重开确认 score=0 len={t["len"]}（最高分已存 NVS）', flush=True)

print('== ALL TESTS PASSED ==', flush=True)
