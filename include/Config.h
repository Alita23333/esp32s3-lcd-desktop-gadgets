#pragma once

// ============================================================
//  双应用固件：GP11 短接 GND = GPU 监控；悬空 = 桌面日历
//  开机检测一次，切换后按 RST 或重新上电生效
// ============================================================

// ================== 模式选择引脚 ==================
#define PIN_MODE_SELECT  11    // 短接到相邻 GND = GPU 监控；悬空(内部上拉) = 日历

// ================== 电源/发热控制 ==================
#define ENABLE_WIFI      1     // 两个应用都需要 WiFi
#define BACKLIGHT_DAY    60    // 日历：白天（日出~日落）背光占空比 0-100
#define BACKLIGHT_NIGHT  10    // 日历：夜间背光占空比（官方示例约 10%）
#define BACKLIGHT_MONITOR 40   // GPU 监控模式背光（射频常开本就热，别太高）

// ================== WiFi ==================
#ifndef WIFI_SSID
#define WIFI_SSID "your-2.4g-ssid"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "your-password"
#endif
// WiFi 主机名：显示在路由器客户端列表里，方便辨认设备
#define WIFI_HOSTNAME "ESP32-Calendar"

// ================== 位置配置 ==================
// 位置获取链：手动配置 > ipip.net(省->省会坐标) > ip-api 兜底
// IP 定位数据库在国内常不准，ipip 的中国库最靠谱（实测正确返回黑龙江），
// 但只到省级——映射到省会城市。如需精确到区县，把 CITY_MANUAL 改为 1 并填坐标。
#define CITY_MANUAL 0
#define CITY_NAME   "哈尔滨"
#define CITY_LAT    45.8038f    // 哈尔滨
#define CITY_LON    126.5349f

// ================== 时间同步 ==================
#define TZ_INFO       "CST-8"          // POSIX 格式：CST-8 即 UTC+8
#define NTP_SERVER_1  "ntp.aliyun.com"
#define NTP_SERVER_2  "pool.ntp.org"

// ================== 天气 ==================
// IP 定位（免密钥，返回中文城市名 + 经纬度）
#define GEO_URL  "http://ip-api.com/json/?lang=zh-CN&fields=status,city,lat,lon"
#define WEATHER_OK_MS    (20UL * 60 * 1000)   // 天气获取成功后的刷新间隔
#define WEATHER_RETRY_MS (60UL * 1000)        // 天气失败重试间隔
#define GEO_RETRY_MS     (10UL * 60 * 1000)   // IP 定位失败重试间隔

// ============================================================
//  专注钟（独立固件）：番茄钟 + 陀螺仪翻面暂停 + TF 卡记录
//  按键（低电平触发，内部上拉，按键接引脚↔GND，无需电阻）
// ============================================================
#define PIN_BTN_START   11      // 2048=上 / 专注钟=开始暂停（与相邻 GND 好接线）
#define PIN_BTN_RESET   10      // 2048=下 / 专注钟=重置或预设
#define PIN_BTN_SKIP    9       // 2048=左 / 专注钟=跳过休息
#define PIN_BTN_RIGHT   8       // 2048=右（第四方向键）
#define FOCUS_PRESETS   {25, 45, 5, 15}   // 可选专注时长（分钟，循环切换）
#define BREAK_SHORT_MIN 5       // 短休（分钟）
#define BREAK_LONG_MIN  15      // 长休（分钟）
#define LONG_BREAK_EVERY 4      // 每完成 N 个专注 -> 长休
#define BACKLIGHT_FOCUS  35     // 本应用背光

// ================== WiFi 雷达（独立固件） ==================
#define RADAR_RESCAN_MS  5000    // 每轮扫描完成后，间隔多久重新扫描

// ================== 可选功能 ==================
#define ENABLE_BLE      0
#define BLE_DEVICE_NAME "ESP32-S3-LCD-1.47B"

// ================== 配色 (RGB565) ==================
#define C_BG        0x0000    // 背景：黑
#define C_HEAD_BG   0x2124    // 分隔线：深灰
#define C_BAR_BG    0x2104    // 进度条底：暗灰（监控用）
#define C_TEXT      0xFFFF    // 文字：白
#define C_DIM       0x8410    // 深灰（图标细节用）
#define C_YELLOW    0xFFE0    // 太阳/闪电：黄
#define C_BLUE      0x3BFF    // 雨滴：亮蓝
#define C_GREEN     0x07E0    // 绿
#define C_CYAN      0x07FF    // 青（监控显存条）
#define C_ORANGE    0xFD20    // 橙
#define C_RED       0xF800    // 红
