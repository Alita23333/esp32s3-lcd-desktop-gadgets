#include "UdpMon.hpp"
#include <WiFiUdp.h>
#include "Config.h"
#include "Wifi.hpp"

// 数据包格式（主机端 pcmon_host.py）：
//   PCMON,<seq>,<util%>,<powerW*10>,<tempC>,<vramMB>,<vramTotalMB>,<powerLimW*10>

static WiFiUDP s_udp;
static char s_buf[144];
static uint16_t s_port = 0;
static bool s_started = false;

void UdpMon_Begin(uint16_t port) {
  s_port = port;
  // 注意：此时 lwIP 栈可能尚未初始化（WiFi 未启动），不能直接 bind，
  // 延迟到 WiFi 连接成功后在 Loop 中启动，否则 tcpip 断言崩溃
}

void UdpMon_Loop(GpuData& d) {
  bool conn = Wifi_IsConnected();
  if (s_started && !conn) {
    s_started = false;   // WiFi 断开，重连后需重新绑定端口
    return;
  }
  if (!s_started) {
    if (!conn) return;
    s_udp.begin(s_port);
    s_started = true;
    Serial.printf("[UDP] listening on :%u\n", s_port);
  }

  int n;
  while ((n = s_udp.parsePacket()) > 0) {
    int len = s_udp.read(s_buf, sizeof(s_buf) - 1);
    if (len <= 0) continue;
    s_buf[len] = '\0';

    if (strncmp(s_buf, "PCMON,", 6) != 0) continue;

    // 解析 7 个整数字段
    int v[7] = {0};
    int idx = 0;
    char* tok = strtok(s_buf + 6, ",");
    while (tok && idx < 7) {
      v[idx++] = atoi(tok);
      tok = strtok(nullptr, ",");
    }
    if (idx != 7) continue;

    d.seq         = (uint16_t)v[0];
    d.util        = (uint16_t)v[1];
    d.powerW      = v[2] / 10.0f;
    d.tempC       = (uint16_t)v[3];
    d.vramMB      = (uint32_t)v[4];
    d.vramTotalMB = (uint32_t)v[5];
    d.powerLimW   = v[6] / 10.0f;
    d.lastRx      = millis();
    d.ever        = true;
  }
}

bool UdpMon_IsFresh(const GpuData& d) {
  return d.ever && (millis() - d.lastRx) < DATA_STALE_MS;
}
