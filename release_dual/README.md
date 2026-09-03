# 双应用固件（GPU 监控 + 桌面日历）— 发布包

一颗灯珠两种人生：**GP11 短接 GND = GPU 监控；悬空 = 桌面日历**。

## 文件
| 文件 | 用途 |
|---|---|
| `dual-app-firmware.bin` | 合并固件（引导+分区+双应用），刷入即用 |
| `flash_firmware.bat` | 一键刷固件（默认 COM5，可在文件顶部改） |

## 模式切换
| GP11 状态 | 应用 | 说明 |
|---|---|---|
| **悬空**（默认） | 桌面日历 | NTP 时间 + 哈尔滨 IP 定位 + Open-Meteo 天气，射频休眠低温 |
| **短接到相邻 GND** | GPU 监控 | 需电脑运行 `pcmon/pcmon_host.py`，射频常开 |

- 模式在**开机时检测一次**：改变短接状态后，**按一下 RST 键**（或重新上电）即切换
- 用一根杜邦线/镊子把右侧排针的 **GP11 和相邻的 GND** 短接即可

## 各模式说明
### 日历模式
- 上电自动连 WiFi（SSID 已烧入），数秒内显示时间，约 10 秒后天气到位
- 亮度随日出日落自动调节；LED 关闭；屏幕变化驱动刷新（无闪烁）
- 位置：自动（ipip 省级定位→省会坐标）；需区县级精度改 `Config.h` 的 `CITY_MANUAL=1`

### GPU 监控模式
- 需电脑端运行采集脚本：`pcmon\pcmon_host.bat`（见 `pcmon/README.md`）
- 显示 GPU 占用/功耗/温度/显存，2Hz 实时刷新
- **注意：此模式射频常开，板温会明显高于日历模式，属正常现象**

## 刷机
双击 `flash_firmware.bat`，或手动：
```
python esptool.py --chip esp32s3 --port COM5 --baud 460800 write_flash 0x0 dual-app-firmware.bin
```

## 相关发布包
- `../release_calendar/` — 纯日历单应用固件（无 GP11 切换）
- `../release/` — 纯 GPU 监控单应用固件（旧版）

## 排错
- 刷完屏幕全黑 / 灯不亮：重新插拔一次 USB（可能卡在下载模式没退出）
- 一直连不上串口：按住 BOOT 键再插 USB，然后重试
- 监控模式显示 WAIT：确认主机脚本在跑；广播不通时把脚本里 `BROADCAST=False`
  并把 `TARGET_IP` 改成副屏 IP（监控模式页脚不显示 IP 时可从路由器客户端列表
  找主机名 **ESP32-Calendar**）
