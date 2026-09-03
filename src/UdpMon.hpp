#pragma once
#include <Arduino.h>

// GPU 监控数据（由主机 pcmon_host.py 通过 UDP 广播）
struct GpuData {
  uint16_t util = 0;          // GPU 占用率 %
  float    powerW = 0.0f;     // 功耗 W
  float    powerLimW = 0.0f;  // 功耗墙 W（0 = 主机未提供）
  uint16_t tempC = 0;         // 温度 ℃
  uint32_t vramMB = 0;        // 显存已用 MB
  uint32_t vramTotalMB = 0;   // 显存总量 MB
  uint16_t seq = 0;           // 包序号
  uint32_t lastRx = 0;        // 最后收包时刻 millis()
  bool     ever = false;      // 是否收到过包
};

// 开始监听 UDP 端口
void UdpMon_Begin(uint16_t port);

// 非阻塞收包解析（每轮 loop 调用）
void UdpMon_Loop(GpuData& d);

// 数据是否新鲜（收到过且未超时）
bool UdpMon_IsFresh(const GpuData& d);
