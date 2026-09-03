# GPU 性能监控副屏 — 发布包

> 💡 需要日历 + GPU 监控二合一的固件（GP11 短接 GND 切换）？见 `../release_dual/`

| 文件 | 用途 |
|---|---|
| `pcmon_host.bat` | **电脑端启动器**：双击开始向副屏广播 GPU 数据（手动运行，无自启；关窗即停） |
| `gpu-monitor-firmware.bin` | **开发板固件**（合并版：引导+分区+应用），刷入后即用 |
| `flash_firmware.bat` | 一键刷固件到板子（默认 COM5，可在文件顶部改） |

## 日常使用
1. 板子上电（自动连 WiFi，屏幕页脚显示其 IP）
2. 双击 `pcmon_host.bat` → 屏幕显示 GPU 占用 / 功耗 / 温度 / 显存，2Hz 刷新

## 以后重新刷固件
双击 `flash_firmware.bat` 即可（esptool 自动触发下载模式，无需按 BOOT）。
或手动执行：
```
python esptool.py --chip esp32s3 --port COM5 --baud 460800 write_flash 0x0 gpu-monitor-firmware.bin
```

## 说明
- 刷入前先在 `include/Config.h` 填好你的 WiFi SSID/密码再编译
  （编译：项目根目录 `.venv\Scripts\platformio.exe run`，产物在 `.pio\build\esp32s3\`）
- 启动器依赖项目内的 `.venv`（含 nvidia-ml-py）；整个项目文件夹整体移动不影响使用
- 广播不通（路由器开 AP 隔离）时：编辑 `pcmon\pcmon_host.py` 顶部，
  设 `BROADCAST = False` 并将 `TARGET_IP` 改为屏幕页脚显示的 IP
- 详细文档见项目根目录 `pcmon\README.md`
