# ESP32-S3-LCD-1.47B — 桌面小玩意儿合集

一块 Waveshare ESP32-S3-LCD-1.47B（172×320 屏 + ESP32-S3R8）上的几个独立小固件。每个都能单独刷进去用，共享同一套驱动（屏幕偏移修正、LED 消隐、WiFi）。

## 固件列表

### 桌面日历
联网自动显示时间、日期、天气。NTP 对时，IP 自动定位省会，天气用 Open-Meteo（免密钥）。亮度跟着日出日落自动调，LED 关，屏幕没变化就不重绘、不闪。

### 专注钟
番茄钟。大字倒计时 + 实体按键，板载陀螺仪做到**翻面自动暂停**（翻回去继续）。完成的次数/分钟存进 NVS 掉电不丢，插张 TF 卡还往 `focuslog.csv` 里记每次专注。射频休眠，低温，电池友好。

### 体感贪吃蛇
倾斜板子控制蛇的方向（陀螺仪），复古贪吃蛇。撞墙/撞自己结束，最高分存 NVS。也可以用串口 `WASD` 玩。

### 2048 体感版
经典 2048。倾斜板子或 4 颗方向键操作，分数和最高分掉电保存，合并到 2048 会提示。

### WiFi 雷达
桌面雷达动画。附近 WiFi 按 SSID 哈希分布在雷达上、按信号强度定半径，扫描线扫过就高亮。不连任何 AP，纯侦测，每 5 秒重扫。适合看看房间 WiFi 环境或判断路由器在不在线。

### GPU 监控副屏
配电脑端的 `pcmon/pcmon_host.py` 用。NVIDIA GPU 的占用/功耗/温度/显存经 WiFi UDP 广播到副屏实时显示。注意这个模式射频常开会热点。

### 双应用固件（GPU 监控 + 日历）
上面两个打包在一起，用 GP11 短接 GND 切到监控、悬空是日历，开机时检测一次。

## 怎么刷

每个固件所在文件夹里都有一个 `flash_firmware.bat`，双击就行（板子 USB 接电脑，默认 COM5）。手动刷也行：

```
python esptool.py --chip esp32s3 --port COM5 --baud 460800 write_flash 0x0 xxx-firmware.bin
```

## 按键

按键一端接引脚、一端接 GND（内部上拉，不用电阻）：

| 引脚 | 功能 |
|---|---|
| GP11 | 开始 / 上（和旁边的 GND 最好接） |
| GP10 | 重开 / 下 |
| GP9 | 跳过 / 左 |
| GP8 | 右 |

焊了键当然好，没焊就镊子短接引脚↔GND 也能当按键。

## 下载固件

不想自己编译的话，去 [Releases](https://github.com/Alita23333/esp32s3-lcd-desktop-gadgets/releases) 拿现成的。每个固件打进了一个 zip（`.bin` + 刷机脚本 + 说明），右键下载解压就能用。

## 项目结构

- `src/` 固件源码（每个应用一个 .cpp/.hpp，`platformio.ini` 里 `build_src_filter` 控制编译哪个）
- `release_*/` 每个固件的成品 .bin + 刷机脚本 + 说明
- `pcmon/` 电脑端 GPU 采集脚本（Python，`pcmon_host.py` + 启动器）
- `test_snake.py` `test_radar.py` 串口自动测试脚本，刷完可以自己跑一遍

## 依赖

- 固件：PlatformIO + espressif32 平台、Adafruit GFX、U8g2、ArduinoJson、NimBLE（按需）
- 电脑端 GPU 监控：`pip install nvidia-ml-py`

WiFi 是 2.4GHz，SSID/密码在 `include/Config.h` 里改。
