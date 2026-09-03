#pragma once
#include <Arduino.h>

// 天气/定位数据（由 Weather_Loop 自动刷新）
struct WeatherData {
  // IP 定位
  bool  geoOk = false;
  float lat = 0, lon = 0;
  char  city[24] = "";          // 中文城市名（UTF-8）

  // 天气（Open-Meteo）
  bool    valid = false;        // 曾获取到有效数据
  float   tempC = 0;
  uint8_t humidity = 0;
  uint8_t code = 0;             // WMO 天气码
  float   windKmh = 0;
  float   tMax[2] = {0, 0};     // 今天/明天最高温
  float   tMin[2] = {0, 0};     // 今天/明天最低温
  char    sunrise[6] = "";      // "HH:MM"
  char    sunset[6]  = "";

  // 同步状态
  uint32_t lastOkMs = 0;
  char     syncAt[6] = "";      // 最后成功同步时刻 "HH:MM"
};

void Weather_Begin();
void Weather_Loop(WeatherData& d);   // 内部含定位/天气定时刷新（联网时阻塞数秒）
