#include "Wifi.hpp"
#include "Config.h"
#include <WiFi.h>

static bool s_configured = false;
static bool s_connected = false;
static String s_ip = "";

void Wifi_Init(bool modemSleep) {
  if (String(WIFI_SSID) == "your-router-ssid" || strlen(WIFI_SSID) == 0) {
    Serial.println("[WiFi] 未配置 SSID，跳过 WiFi");
    s_configured = false;
    return;
  }
  s_configured = true;
  WiFi.mode(WIFI_STA);
  // 休眠策略由应用运行时决定：
  //   modemSleep=true  -> 调制解调器休眠，定期同步场景省电降温（日历）
  //   modemSleep=false -> 射频常开，持续接收 UDP 不可丢包（GPU 监控）
  WiFi.setSleep(modemSleep);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(WIFI_HOSTNAME);   // 路由器客户端列表里可辨认
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WiFi] 开始连接 SSID=%s (modemSleep=%d)\n", WIFI_SSID, modemSleep ? 1 : 0);
}

bool Wifi_IsConfigured() { return s_configured; }
bool Wifi_IsConnected() { return s_connected; }
String Wifi_GetIP() { return s_ip; }

void Wifi_Loop() {
  if (!s_configured) return;
  wl_status_t st = WiFi.status();

  // 诊断：10 秒仍未连上 -> 打印连接状态与附近网络列表（仅一次）
  static uint32_t t0 = 0, lastLog = 0;
  static bool scanned = false;
  if (st != WL_CONNECTED) {
    uint32_t now = millis();
    if (t0 == 0) t0 = now;
    if (now - lastLog > 3000) {
      lastLog = now;
      Serial.printf("[WiFi] 连接中 status=%d\n", st);
    }
    if (!scanned && now - t0 > 10000) {
      scanned = true;
      Serial.printf("[WiFi] 10s 未连上，扫描附近网络（目标 SSID=%s）：\n", WIFI_SSID);
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n; i++) {
        Serial.printf("  %s  ch%d %ddBm%s\n", WiFi.SSID(i).c_str(), WiFi.channel(i),
                      WiFi.RSSI(i), WiFi.channel(i) > 14 ? "  <- 5GHz(ESP32不支持)" : "");
      }
      if (n <= 0) Serial.println("  (无结果)");
      WiFi.scanDelete();
    }
  }

  if (st == WL_CONNECTED && !s_connected) {
    s_connected = true;
    s_ip = WiFi.localIP().toString();
    Serial.printf("[WiFi] 已连接，IP=%s\n", s_ip.c_str());
  } else if (st != WL_CONNECTED && s_connected) {
    s_connected = false;
    Serial.println("[WiFi] 连接断开");
  }
}
