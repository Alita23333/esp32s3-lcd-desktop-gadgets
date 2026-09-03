#include "Dashboard.hpp"
#include "Config.h"
#include "Wifi.hpp"

// ================= 布局常量 (172x320 竖屏) =================
static const int16_t HEAD_H   = 20;
static const int16_t BLOCK_T[4] = {24, 92, 160, 228};  // 四个指标块顶部
static const int16_t FOOT_Y   = 302;

// 块内偏移
static const int16_t OFF_LABEL = 0;
static const int16_t OFF_VALUE = 11;
static const int16_t OFF_BAR   = 40;
static const int16_t OFF_SUB   = 52;
static const int16_t BAR_W     = 164;
static const int16_t BAR_H     = 8;

// RTX 4080 Laptop 典型最大 TGP，主机读不到功耗墙时兜底
static const float FALLBACK_POWER_LIM = 175.0f;

// ================= 内部状态 =================
static int16_t s_lastFill[4] = {-1, -1, -1, -1};   // 各进度条上次填充像素
static int8_t  s_lastLive = -1;                    // 头部 LIVE/WAIT 状态
static int8_t  s_lastWifi = -1;                    // WiFi 状态点
static char    s_footerL[40] = "";                 // 页脚左（IP/WiFi 状态）
static char    s_footerR[16] = "";                 // 页脚右（序号）
static bool    s_vramSubInit = false;

// ---------- 基础绘制辅助 ----------
static void drawLabel(Lcd& lcd, int blockIdx, const char* text) {
  lcd.setTextSize(1);
  lcd.setTextColor(C_DIM, C_BG);
  lcd.setCursor(4, BLOCK_T[blockIdx] + OFF_LABEL);
  lcd.print(text);
}

static void drawValueText(Lcd& lcd, int blockIdx, const char* text, uint16_t color) {
  lcd.setTextSize(3);
  lcd.setTextColor(color, C_BG);
  lcd.setCursor(4, BLOCK_T[blockIdx] + OFF_VALUE);
  lcd.print(text);
}

static void drawBar(Lcd& lcd, int blockIdx, int16_t fillPx, uint16_t color) {
  if (fillPx == s_lastFill[blockIdx]) return;
  int16_t y = BLOCK_T[blockIdx] + OFF_BAR;
  lcd.fillRect(4, y, BAR_W, BAR_H, C_BAR_BG);
  if (fillPx > 0) lcd.fillRect(4, y, fillPx, BAR_H, color);
  s_lastFill[blockIdx] = fillPx;
}

static int clampPct(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

// ---------- 页脚 ----------
static void drawFooter(Lcd& lcd, bool wifiOk, uint16_t seq) {
  char l[40], r[16];
  if (wifiOk) {
    snprintf(l, sizeof(l), "IP %s", Wifi_GetIP().c_str());
  } else {
#if ENABLE_WIFI
    snprintf(l, sizeof(l), "WiFi DISCONNECTED");
#else
    snprintf(l, sizeof(l), "WIFI OFF (STANDBY)");
#endif
  }
  snprintf(r, sizeof(r), "#%u", seq);

  if (strcmp(l, s_footerL) != 0) {
    lcd.fillRect(0, FOOT_Y, LCD_W / 2, 18, C_BG);
    lcd.setTextSize(1);
    lcd.setTextColor(C_DIM, C_BG);
    lcd.setCursor(4, FOOT_Y + 4);
    lcd.print(l);
    snprintf(s_footerL, sizeof(s_footerL), "%s", l);
  }
  if (strcmp(r, s_footerR) != 0) {
    lcd.fillRect(LCD_W / 2, FOOT_Y, LCD_W / 2, 18, C_BG);
    lcd.setTextSize(1);
    lcd.setTextColor(C_DIM, C_BG);
    lcd.setCursor(LCD_W - 4 - strlen(r) * 6, FOOT_Y + 4);
    lcd.print(r);
    snprintf(s_footerR, sizeof(s_footerR), "%s", r);
  }
}

// ---------- 头部状态 ----------
static void drawHeaderStatus(Lcd& lcd, bool live, bool wifiOk) {
  int8_t st = live ? 1 : 0;
  if (st != s_lastLive) {
    lcd.fillRect(126, 0, 32, HEAD_H, C_HEAD_BG);
    lcd.setTextSize(1);
    lcd.setTextColor(live ? C_GREEN : C_ORANGE, C_HEAD_BG);
    lcd.setCursor(126, 6);
    lcd.print(live ? "LIVE" : "WAIT");
    s_lastLive = st;
  }
  int8_t w = wifiOk ? 1 : 0;
  if (w != s_lastWifi) {
    lcd.fillRect(162, 7, 6, 6, wifiOk ? C_GREEN : C_RED);
    s_lastWifi = w;
  }
}

// ================= 公开接口 =================
void Dash_Begin(Lcd& lcd) {
  lcd.fillRect(0, 0, LCD_W, LCD_H, C_BG);

  // 头部
  lcd.fillRect(0, 0, LCD_W, HEAD_H, C_HEAD_BG);
  lcd.setTextSize(1);
  lcd.setTextColor(C_TEXT, C_HEAD_BG);
  lcd.setCursor(4, 6);
  lcd.print("GPU MONITOR");

  // 标签
  drawLabel(lcd, 0, "GPU UTIL");
  drawLabel(lcd, 1, "POWER");
  drawLabel(lcd, 2, "TEMP");
  drawLabel(lcd, 3, "VRAM");

  // 页脚分隔线
  lcd.fillRect(0, FOOT_Y - 2, LCD_W, 1, C_HEAD_BG);

  s_lastLive = -1; s_lastWifi = -1;
  for (int i = 0; i < 4; i++) s_lastFill[i] = -1;
  s_footerL[0] = 0; s_footerR[0] = 0;
  s_vramSubInit = false;
}

void Dash_Render(Lcd& lcd, const GpuData& d, bool wifiOk) {
  bool fresh = UdpMon_IsFresh(d);
  char buf[24];

  drawHeaderStatus(lcd, fresh, wifiOk);

  // ---- 1. GPU 占用 ----
  if (fresh) {
    snprintf(buf, sizeof(buf), "%3u%%", d.util);
    drawValueText(lcd, 0, buf, C_TEXT);
    drawBar(lcd, 0, (BAR_W * clampPct(d.util)) / 100, C_GREEN);
  } else {
    drawValueText(lcd, 0, " --%", C_DIM);
    drawBar(lcd, 0, 0, C_GREEN);
  }

  // ---- 2. 功耗 ----
  if (fresh) {
    float lim = (d.powerLimW > 1.0f) ? d.powerLimW : FALLBACK_POWER_LIM;
    float ratio = d.powerW / lim;
    uint16_t c = (ratio < 0.6f) ? C_GREEN : (ratio < 0.85f ? C_ORANGE : C_RED);
    snprintf(buf, sizeof(buf), "%5.1f W", d.powerW);
    drawValueText(lcd, 1, buf, C_TEXT);
    drawBar(lcd, 1, (int16_t)(BAR_W * (ratio < 0 ? 0 : (ratio > 1 ? 1 : ratio))), c);
  } else {
    drawValueText(lcd, 1, "   -- W", C_DIM);
    drawBar(lcd, 1, 0, C_GREEN);
  }

  // ---- 3. 温度 ----
  if (fresh) {
    uint16_t c = (d.tempC < 70) ? C_GREEN : (d.tempC < 85 ? C_ORANGE : C_RED);
    snprintf(buf, sizeof(buf), "%3u C", d.tempC);
    drawValueText(lcd, 2, buf, c);
    int t = (int)d.tempC - 30;
    if (t < 0) t = 0;
    if (t > 65) t = 65;
    drawBar(lcd, 2, (BAR_W * t) / 65, c);
  } else {
    drawValueText(lcd, 2, " -- C", C_DIM);
    drawBar(lcd, 2, 0, C_GREEN);
  }

  // ---- 4. 显存 ----
  if (fresh) {
    float gb = d.vramMB / 1024.0f;
    snprintf(buf, sizeof(buf), "%5.1f G", gb);
    drawValueText(lcd, 3, buf, C_TEXT);
    if (d.vramTotalMB > 0) {
      drawBar(lcd, 3, (int16_t)((uint64_t)BAR_W * d.vramMB / d.vramTotalMB), C_CYAN);
      snprintf(buf, sizeof(buf), "%u/%u MB", (unsigned)d.vramMB, (unsigned)d.vramTotalMB);
      lcd.setTextSize(1);
      lcd.setTextColor(C_DIM, C_BG);
      lcd.setCursor(4, BLOCK_T[3] + OFF_SUB);
      lcd.print(buf);
      s_vramSubInit = true;
    }
  } else {
    drawValueText(lcd, 3, "   -- G", C_DIM);
    drawBar(lcd, 3, 0, C_CYAN);
    if (s_vramSubInit) {
      lcd.fillRect(4, BLOCK_T[3] + OFF_SUB, 120, 8, C_BG);
      s_vramSubInit = false;
    }
  }

  drawFooter(lcd, wifiOk, fresh ? d.seq : 0);
}
