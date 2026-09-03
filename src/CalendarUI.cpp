#include "CalendarUI.hpp"
#include "Config.h"
#include <U8g2_for_Adafruit_GFX.h>

static U8G2_FOR_ADAFRUIT_GFX s_u8;

// ================= 字体 =================
#define F_CN      u8g2_font_wqy16_t_gb2312    // 16px 中文（全量 GB2312）
#define F_CLOCK   u8g2_font_logisoso42_tr     // 42px 时钟数字
#define F_TEMP    u8g2_font_logisoso32_tr     // 32px 温度数字

static const char* kWeek[] = {"日", "一", "二", "三", "四", "五", "六"};

// ================= WMO 天气码 -> 中文/图标 =================
static const char* wxText(uint8_t c) {
  if (c == 0)  return "晴";
  if (c <= 2)  return "多云";
  if (c == 3)  return "阴";
  if (c == 45 || c == 48) return "雾";
  if (c <= 57) return "毛毛雨";             // 51..57
  if (c == 61) return "小雨";
  if (c == 63) return "中雨";
  if (c == 65) return "大雨";
  if (c <= 67) return "冻雨";               // 66,67
  if (c <= 77) return (c == 75) ? "大雪" : "雪";   // 71..77
  if (c <= 82) return "阵雨";               // 80..82
  if (c <= 86) return "阵雪";               // 85,86
  return "雷雨";                            // 95..99
}

enum WxIcon { WXI_SUN, WXI_PARTLY, WXI_CLOUD, WXI_FOG, WXI_RAIN, WXI_SNOW, WXI_STORM };

static WxIcon wxIconOf(uint8_t c) {
  if (c == 0)  return WXI_SUN;
  if (c <= 2)  return WXI_PARTLY;
  if (c == 3)  return WXI_CLOUD;
  if (c == 45 || c == 48) return WXI_FOG;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return WXI_RAIN;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return WXI_SNOW;
  return WXI_STORM;                          // 95..99
}

// ---- 云朵（y 为云顶）----
static void drawCloud(Lcd& lcd, int16_t x, int16_t y) {
  lcd.fillCircle(x + 15, y + 10, 11, C_TEXT);
  lcd.fillCircle(x + 30, y + 6, 14, C_TEXT);
  lcd.fillRect(x + 5, y + 10, 40, 12, C_TEXT);
}

// ---- 天气图标（52x52 区域，左上角 x,y）----
static void drawIcon(Lcd& lcd, WxIcon t, int16_t x, int16_t y) {
  switch (t) {
    case WXI_SUN: {
      int16_t cx = x + 26, cy = y + 24;
      lcd.fillCircle(cx, cy, 11, C_YELLOW);
      for (int i = 0; i < 8; i++) {
        float a = i * 0.785398f;   // PI/4
        lcd.drawLine(cx + (int)(cosf(a) * 16), cy + (int)(sinf(a) * 16),
                     cx + (int)(cosf(a) * 23), cy + (int)(sinf(a) * 23), C_YELLOW);
      }
      break;
    }
    case WXI_PARTLY:
      lcd.fillCircle(x + 36, y + 14, 8, C_YELLOW);
      drawCloud(lcd, x, y + 20);
      break;
    case WXI_CLOUD:
      drawCloud(lcd, x, y + 12);
      break;
    case WXI_FOG:
      drawCloud(lcd, x, y + 4);
      for (int i = 0; i < 3; i++)
        lcd.fillRect(x + 4 + (i % 2) * 8, y + 36 + i * 6, 40, 3, C_DIM);
      break;
    case WXI_RAIN:
      drawCloud(lcd, x, y);
      for (int i = 0; i < 3; i++)
        lcd.drawLine(x + 16 + i * 11, y + 34, x + 13 + i * 11, y + 46, C_BLUE);
      break;
    case WXI_SNOW:
      drawCloud(lcd, x, y);
      for (int i = 0; i < 3; i++)
        lcd.fillCircle(x + 17 + i * 10, y + 40, 2, C_TEXT);
      break;
    case WXI_STORM:
      drawCloud(lcd, x, y);
      lcd.fillTriangle(x + 26, y + 28, x + 16, y + 44, x + 26, y + 42, C_YELLOW);
      lcd.fillTriangle(x + 24, y + 42, x + 18, y + 54, x + 32, y + 40, C_YELLOW);
      break;
  }
}

// ================= 文本辅助 =================
static void cnText(Lcd& lcd, int16_t x, int16_t baselineY, const char* s, uint16_t fg) {
  s_u8.setFont(F_CN);
  s_u8.setForegroundColor(fg);
  s_u8.setBackgroundColor(C_BG);
  s_u8.setCursor(x, baselineY);
  s_u8.print(s);
}

static int16_t cnWidth(const char* s) {
  s_u8.setFont(F_CN);
  return s_u8.getUTF8Width(s);
}

// ================= 变化检测状态 =================
static long    s_dateKey  = -1;      // 年月日（0=时间未同步）
static int     s_lastMin  = -1;      // 时钟分钟
static bool    s_lastTimeOk = false;
static bool    s_wxInit   = false;   // 天气块首次绘制标记
static struct {
  bool    valid;
  uint8_t code;
  int16_t t10, w10;
  uint8_t hum;
  int16_t mx0, mn0, mx1, mn1;
} s_wxSig;
static char    s_footL[32] = "";
static char    s_footR[8]  = "";
static bool    s_footInit  = false;
static int     s_lastDuty  = -1;   // 当前背光档位（-1=未设置）

// 背光昼夜自适应：优先用真实日出日落时刻，天气未同步时按 7:00-19:00 粗略判断
static void updateBacklight(Lcd& lcd, bool timeOk, const struct tm& t, const WeatherData& wx) {
  if (!timeOk) return;   // 时间未同步前保持初始亮度
  int curM = t.tm_hour * 60 + t.tm_min;
  bool isDay;
  if (wx.valid && wx.sunrise[0] && wx.sunset[0]) {
    int sr = atoi(wx.sunrise) * 60 + atoi(wx.sunrise + 3);
    int ss = atoi(wx.sunset) * 60 + atoi(wx.sunset + 3);
    isDay = (curM >= sr && curM < ss);
  } else {
    isDay = (t.tm_hour >= 7 && t.tm_hour < 19);
  }
  int duty = isDay ? BACKLIGHT_DAY : BACKLIGHT_NIGHT;
  if (duty != s_lastDuty) {
    s_lastDuty = duty;
    lcd.setBacklight((uint8_t)duty);
  }
}

// ================= 天气主区（y96..240，含今明预报） =================
static void drawWeatherBlock(Lcd& lcd, const WeatherData& wx, bool wifiOk) {
  lcd.fillRect(0, 96, LCD_W, 144, C_BG);

  if (!wx.valid) {
    const char* tip = wifiOk ? "天气获取中..." : "天气需要网络";
    cnText(lcd, (LCD_W - cnWidth(tip)) / 2, 140, tip, C_TEXT);
    return;
  }

  drawIcon(lcd, wxIconOf(wx.code), 8, 104);

  // 温度大数字 + ℃
  char buf[24];
  snprintf(buf, sizeof(buf), "%d", (int)lroundf(wx.tempC));
  s_u8.setFont(F_TEMP);
  s_u8.setForegroundColor(C_TEXT);
  s_u8.setBackgroundColor(C_BG);
  s_u8.setCursor(72, 148);
  s_u8.print(buf);
  int16_t tx = 72 + s_u8.getUTF8Width(buf) + 4;
  cnText(lcd, tx, 148, "℃", C_TEXT);

  // 天气现象 / 湿度 / 风
  cnText(lcd, 8, 176, wxText(wx.code), C_TEXT);
  snprintf(buf, sizeof(buf), "湿度 %d%%", wx.humidity);
  cnText(lcd, 8, 200, buf, C_TEXT);
  snprintf(buf, sizeof(buf), "风 %.0f km/h", (double)wx.windKmh);
  cnText(lcd, 8, 224, buf, C_TEXT);

  // 今明预报
  snprintf(buf, sizeof(buf), "今天 %d/%d℃",
           (int)lroundf(wx.tMax[0]), (int)lroundf(wx.tMin[0]));
  cnText(lcd, 8, 268, buf, C_TEXT);
  snprintf(buf, sizeof(buf), "明天 %d/%d℃",
           (int)lroundf(wx.tMax[1]), (int)lroundf(wx.tMin[1]));
  cnText(lcd, 8, 292, buf, C_TEXT);
}

// ================= 公开接口 =================
void Cal_Begin(Lcd& lcd) {
  s_u8.begin(lcd);
  lcd.fillRect(0, 0, LCD_W, LCD_H, C_BG);
  // 静态分隔线（只画一次）
  lcd.fillRect(0, 92,  LCD_W, 1, C_HEAD_BG);
  lcd.fillRect(0, 240, LCD_W, 1, C_HEAD_BG);
  s_dateKey = -1; s_lastMin = -1; s_lastTimeOk = false;
  s_wxInit = false; s_footInit = false;
}

void Cal_Render(Lcd& lcd, const WeatherData& wx, bool wifiOk) {
  struct tm t;
  bool timeOk = getLocalTime(&t, 0);
  char buf[48];

  updateBacklight(lcd, timeOk, t, wx);   // 昼夜自适应亮度

  // ---- 日期行：仅跨天/首次同步时重绘 ----
  long dateKey = timeOk ? (long)(t.tm_year + 1900) * 10000 +
                          (t.tm_mon + 1) * 100 + t.tm_mday : 0;
  if (dateKey != s_dateKey) {
    s_dateKey = dateKey;
    lcd.fillRect(0, 6, LCD_W, 20, C_BG);
    if (timeOk)
      snprintf(buf, sizeof(buf), "%d年%d月%d日 星期%s",
               t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, kWeek[t.tm_wday]);
    else
      snprintf(buf, sizeof(buf), "时间同步中...");
    cnText(lcd, 6, 22, buf, C_TEXT);
  }

  // ---- 大时钟：仅跨分钟/同步状态变化时重绘 ----
  bool needClock = (timeOk != s_lastTimeOk) || (timeOk && t.tm_min != s_lastMin);
  if (needClock) {
    s_lastTimeOk = timeOk;
    s_lastMin = timeOk ? t.tm_min : -1;
    lcd.fillRect(0, 30, LCD_W, 50, C_BG);
    if (timeOk)
      snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    else
      snprintf(buf, sizeof(buf), "--:--");
    s_u8.setFont(F_CLOCK);
    int16_t w = s_u8.getUTF8Width(buf);
    s_u8.setForegroundColor(C_TEXT);
    s_u8.setBackgroundColor(C_BG);
    s_u8.setCursor((LCD_W - w) / 2, 78);
    s_u8.print(buf);
  }

  // ---- 天气区：仅数据变化时重绘（含今明预报）----
  bool needWx = !s_wxInit ||
                s_wxSig.valid != wx.valid || s_wxSig.code != wx.code ||
                s_wxSig.t10 != (int16_t)(wx.tempC * 10) ||
                s_wxSig.hum != wx.humidity ||
                s_wxSig.w10 != (uint16_t)(wx.windKmh * 10) ||
                s_wxSig.mx0 != (int16_t)(wx.tMax[0] * 10) ||
                s_wxSig.mn0 != (int16_t)(wx.tMin[0] * 10) ||
                s_wxSig.mx1 != (int16_t)(wx.tMax[1] * 10) ||
                s_wxSig.mn1 != (int16_t)(wx.tMin[1] * 10);
  if (needWx) {
    s_wxInit = true;
    s_wxSig.valid = wx.valid;
    s_wxSig.code = wx.code;
    s_wxSig.t10 = (int16_t)(wx.tempC * 10);
    s_wxSig.hum = wx.humidity;
    s_wxSig.w10 = (uint16_t)(wx.windKmh * 10);
    s_wxSig.mx0 = (int16_t)(wx.tMax[0] * 10);
    s_wxSig.mn0 = (int16_t)(wx.tMin[0] * 10);
    s_wxSig.mx1 = (int16_t)(wx.tMax[1] * 10);
    s_wxSig.mn1 = (int16_t)(wx.tMin[1] * 10);
    drawWeatherBlock(lcd, wx, wifiOk);
  }

  // ---- 页脚：仅内容变化时重绘 ----
  const char* left = wx.geoOk ? wx.city : (wifiOk ? "定位中..." : "无网络");
  const char* right = wx.valid ? wx.syncAt : "未同步";
  if (!s_footInit || strcmp(left, s_footL) != 0 || strcmp(right, s_footR) != 0) {
    snprintf(s_footL, sizeof(s_footL), "%s", left);
    snprintf(s_footR, sizeof(s_footR), "%s", right);
    s_footInit = true;
    lcd.fillRect(0, 306, LCD_W, 14, C_BG);
    cnText(lcd, 6, 318, s_footL, C_TEXT);
    int16_t rx = LCD_W - 6 - cnWidth(s_footR);
    cnText(lcd, rx, 318, s_footR, C_TEXT);
  }
}
