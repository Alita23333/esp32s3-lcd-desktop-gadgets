# GPU 性能监控副屏 — 使用说明

## 组成
| 部分 | 位置 | 说明 |
|---|---|---|
| 主机端采集 | `pcmon/pcmon_host.py` | 读取 NVIDIA GPU（pynvml），UDP 广播 |
| 副屏固件 | `src/` | ESP32-S3-LCD-1.47B，WiFi 接收 + 仪表盘 |

## 日常使用
1. 副屏上电（自动连 WiFi，屏幕页脚显示其 IP）
2. 电脑运行采集脚本（建议用项目 venv）：
   ```
   .venv\Scripts\python.exe pcmon\pcmon_host.py
   ```
3. 屏幕即显示：GPU 占用 / 功耗 / 温度 / 显存，2Hz 刷新

## 开机自启（可选）
`Win+R` → `shell:startup` → 放入 `pcmon_host.bat`（内容一行）：
```
"C:\Users\Alita\Documents\.zcode\workspace\esp32s3\.venv\Scripts\python.exe" "C:\Users\Alita\Documents\.zcode\workspace\esp32s3\pcmon\pcmon_host.py"
```

## 配置项
- 主机端（脚本顶部）：`SEND_INTERVAL` 发送间隔、`BROADCAST`/`TARGET_IP` 发送方式、`PORT`、`GPU_INDEX`
- 副屏（`include/Config.h`）：`WIFI_SSID`/`WIFI_PASSWORD`、`UDP_PORT`、`DATA_STALE_MS`

## 故障排查
- **屏幕显示 "WAIT" 且页脚 WiFi DISCONNECTED**：检查路由器是否 2.4GHz（ESP32 不支持 5GHz）、SSID/密码是否正确；固件 10 秒连不上会自动扫描并打印附近网络（串口 115200）
- **WiFi 已连但数据不动**：确认主机脚本在运行；若广播不通（路由器开了 AP 隔离），把主机端 `BROADCAST = False` 并将 `TARGET_IP` 设为屏幕页脚显示的 IP
- **烧录**：PlatformIO `pio run -t upload --upload-port COM5`（下载模式）

## 数据协议（UDP :9999，CSV 文本）
```
PCMON,<seq>,<util%>,<powerW*10>,<tempC>,<vramMB>,<vramTotalMB>,<powerLimW*10>
```
序号 65535 回绕；3 秒无包屏幕转 "WAIT" 状态。

## 依赖
- 主机：`pip install nvidia-ml-py`（venv 已装）
- 副屏：PlatformIO + Adafruit GFX Library（lib_deps 已声明）
