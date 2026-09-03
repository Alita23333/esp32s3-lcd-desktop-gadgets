#include "Weather.hpp"
#include "Config.h"
#include "Wifi.hpp"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static uint32_t s_geoTry = 0;
static uint32_t s_wxTry = 0;

void Weather_Begin() {
  s_geoTry = 0;
  s_wxTry = 0;
}

// ---- HTTP GET，返回 200 的响应体 ----
static bool httpGetString(const char* url, bool secure, String& out) {
  HTTPClient http;
  bool ok = false;
  if (secure) {
    WiFiClientSecure cli;
    cli.setInsecure();               // 天气数据非敏感，跳过证书校验
    if (http.begin(cli, url)) {
      int code = http.GET();
      if (code == 200) { out = http.getString(); ok = true; }
      http.end();
    }
  } else {
    if (http.begin(url)) {
      int code = http.GET();
      if (code == 200) { out = http.getString(); ok = true; }
      http.end();
    }
  }
  return ok;
}

// ---- IP 自动定位（中文城市名 + 经纬度）----
static void tryGeolocate(WeatherData& d) {
  Serial.println("[GEO] query...");
  String body;
  if (!httpGetString(GEO_URL, false, body)) {
    Serial.println("[GEO] http fail");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, body)) return;
  const char* status = doc["status"] | "";
  if (strcmp(status, "success") != 0) return;

  const char* city = doc["city"] | "";
  float lat = doc["lat"] | 0.0f;
  float lon = doc["lon"] | 0.0f;

  // 严格校验外部输入：范围合法且不全为零才可用（防止异常数据注入请求）
  bool latOk = (lat > -90.0f && lat < 90.0f);
  bool lonOk = (lon > -180.0f && lon < 180.0f);
  if (!city[0] || !latOk || !lonOk || (lat == 0.0f && lon == 0.0f)) return;

  snprintf(d.city, sizeof(d.city), "%s", city);
  d.lat = lat;
  d.lon = lon;
  d.geoOk = true;
  Serial.printf("[GEO] %s (%.4f, %.4f)\n", d.city, d.lat, d.lon);
}

// ---- ipip.net 定位（中国库最准，但只到省级 -> 映射省会坐标）----
struct ProvCap {
  const char* prov;   // ipip 输出的省名（UTF-8）
  const char* cap;    // 省会城市名（显示用）
  float lat, lon;
};
static const ProvCap kProvCaps[] = {
  {"黑龙江", "哈尔滨",   45.8038f, 126.5349f},
  {"吉林",   "长春",     43.8171f, 125.3235f},
  {"辽宁",   "沈阳",     41.8057f, 123.4315f},
  {"北京",   "北京",     39.9042f, 116.4074f},
  {"天津",   "天津",     39.3434f, 117.3616f},
  {"河北",   "石家庄",   38.0428f, 114.5149f},
  {"山西",   "太原",     37.8706f, 112.5489f},
  {"内蒙古", "呼和浩特", 40.8424f, 111.7490f},
  {"山东",   "济南",     36.6512f, 117.1201f},
  {"河南",   "郑州",     34.7466f, 113.6254f},
  {"江苏",   "南京",     32.0603f, 118.7969f},
  {"上海",   "上海",     31.2304f, 121.4737f},
  {"浙江",   "杭州",     30.2741f, 120.1551f},
  {"安徽",   "合肥",     31.8206f, 117.2272f},
  {"江西",   "南昌",     28.6820f, 115.8579f},
  {"福建",   "福州",     26.0745f, 119.2965f},
  {"湖南",   "长沙",     28.2282f, 112.9388f},
  {"湖北",   "武汉",     30.5928f, 114.3055f},
  {"广东",   "广州",     23.1291f, 113.2644f},
  {"广西",   "南宁",     22.8170f, 108.3665f},
  {"海南",   "海口",     20.0444f, 110.1999f},
  {"重庆",   "重庆",     29.5630f, 106.5516f},
  {"四川",   "成都",     30.5728f, 104.0668f},
  {"贵州",   "贵阳",     26.6470f, 106.6302f},
  {"云南",   "昆明",     24.8801f, 102.8329f},
  {"西藏",   "拉萨",     29.6520f,  91.1721f},
  {"陕西",   "西安",     34.3416f, 108.9398f},
  {"甘肃",   "兰州",     36.0611f, 103.8343f},
  {"宁夏",   "银川",     38.4872f, 106.2309f},
  {"青海",   "西宁",     36.6171f, 101.7782f},
  {"新疆",   "乌鲁木齐", 43.8256f,  87.6168f},
};

static bool tryLocateByIpip(WeatherData& d) {
  String body;
  if (!httpGetString("http://myip.ipip.net", false, body)) return false;

  // 响应形如 "当前 IP：x.x.x.x  来自于：中国 黑龙江   移动"
  int zh = body.indexOf("中国");
  if (zh < 0) return false;
  const char* p = body.c_str() + zh + strlen("中国");
  while (*p == ' ') p++;                       // 跳过空格
  const char* end = strchr(p, ' ');            // 省名到下一个空格
  if (!end || end == p) return false;
  int provLen = (int)(end - p);

  for (size_t i = 0; i < sizeof(kProvCaps) / sizeof(kProvCaps[0]); i++) {
    if ((int)strlen(kProvCaps[i].prov) == provLen &&
        strncmp(p, kProvCaps[i].prov, provLen) == 0) {
      snprintf(d.city, sizeof(d.city), "%s", kProvCaps[i].cap);
      d.lat = kProvCaps[i].lat;
      d.lon = kProvCaps[i].lon;
      d.geoOk = true;
      Serial.printf("[GEO] ipip: %s -> %s (%.4f, %.4f)\n",
                    kProvCaps[i].prov, d.city, d.lat, d.lon);
      return true;
    }
  }
  Serial.println("[GEO] ipip: 未匹配到省份");
  return false;
}

// ---- Open-Meteo 天气（含今天/明天预报）----
static void tryWeather(WeatherData& d) {
  Serial.println("[WX] query...");
  // 请求串由固定常量 + 已过范围校验的经纬度数值拼出
  String url = String(
                   "https://api.open-meteo.com/v1/forecast"
                   "?current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
                   "&daily=sunrise,sunset,temperature_2m_max,temperature_2m_min"
                   "&timezone=auto&forecast_days=2"
                   "&latitude=") +
               String(d.lat, 4) +
               "&longitude=" +
               String(d.lon, 4);

  String body;
  if (!httpGetString(url.c_str(), true, body)) return;

  JsonDocument doc;
  if (deserializeJson(doc, body)) return;

  d.tempC    = doc["current"]["temperature_2m"] | 0.0f;
  d.humidity = doc["current"]["relative_humidity_2m"] | 0;
  d.code     = doc["current"]["weather_code"] | 0;
  d.windKmh  = doc["current"]["wind_speed_10m"] | 0.0f;

  // daily 数组：[0]=今天 [1]=明天
  for (int i = 0; i < 2; i++) {
    d.tMax[i] = doc["daily"]["temperature_2m_max"][i] | 0.0f;
    d.tMin[i] = doc["daily"]["temperature_2m_min"][i] | 0.0f;
  }
  // sunrise/sunset 形如 "2026-09-03T05:45"，截取 HH:MM
  const char* sr = doc["daily"]["sunrise"][0] | "";
  const char* ss = doc["daily"]["sunset"][0] | "";
  if (strlen(sr) >= 16) snprintf(d.sunrise, sizeof(d.sunrise), "%s", sr + 11);
  if (strlen(ss) >= 16) snprintf(d.sunset, sizeof(d.sunset), "%s", ss + 11);

  d.valid = true;
  d.lastOkMs = millis();
  struct tm t;
  if (getLocalTime(&t, 50))
    snprintf(d.syncAt, sizeof(d.syncAt), "%02d:%02d", t.tm_hour, t.tm_min);
  Serial.printf("[WX] %.1fC h%d%% code%d wind %.1fkm/h %s~%s\n",
                d.tempC, d.humidity, d.code, d.windKmh, d.sunrise, d.sunset);
}

void Weather_Loop(WeatherData& d) {
  if (!Wifi_IsConnected()) return;
  uint32_t now = millis();
  if (now < 4000) return;   // WiFi 刚连上，等网络栈稳定

#if CITY_MANUAL
  // 手动配置的位置：直接使用，跳过 IP 定位（IP 定位在国内常不准）
  if (!d.geoOk) {
    snprintf(d.city, sizeof(d.city), "%s", CITY_NAME);
    d.lat = CITY_LAT;
    d.lon = CITY_LON;
    d.geoOk = true;
    Serial.printf("[GEO] 手动配置: %s (%.4f, %.4f)\n", d.city, d.lat, d.lon);
  }
#else
  if (!d.geoOk) {
    // s_geoTry==0 表示从未尝试过 -> 首次立即执行（否则会等满一个重试间隔）
    if (s_geoTry == 0 || now - s_geoTry >= GEO_RETRY_MS) {
      s_geoTry = now;
      // 定位链：ipip（中国省份级，最准）-> ip-api 兜底
      if (!tryLocateByIpip(d)) {
        tryGeolocate(d);
      }
    }
    return;   // 定位成功前不拉天气
  }
#endif

  uint32_t interval = d.valid ? WEATHER_OK_MS : WEATHER_RETRY_MS;
  if (s_wxTry == 0 || now - s_wxTry >= interval) {
    s_wxTry = now;
    tryWeather(d);
  }
}
