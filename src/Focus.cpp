#include "Focus.hpp"
#include "Config.h"
#include "Button.hpp"
#include "Qmi.hpp"
#include <Preferences.h>
#include <SD_MMC.h>
#include <FS.h>
#include <U8g2_for_Adafruit_GFX.h>

static U8G2_FOR_ADAFRUIT_GFX s_u8;

// ================= 外设状态 =================
static bool s_qmiOk = false;      // 陀螺仪就绪（翻面暂停用）
static bool s_sdOk = false;       // TF 卡就绪（专注记录用）
static bool s_faceDown = false;   // 当前朝向（屏幕朝下）
static bool s_flipPaused = false; // 本次暂停是否由翻面触发

// ================= 状态 =================
enum Phase { ST_IDLE, ST_RUN_FOCUS, ST_PAUSE_FOCUS, ST_RUN_BREAK, ST_PAUSE_BREAK };
static Phase s_phase = ST_IDLE;

static uint32_t s_endMs = 0;      // 运行中：结束时刻 (millis)
static uint32_t s_remainMs = 0;   // 暂停中：剩余毫秒
static uint32_t s_totalMs = 0;    // 当前阶段总时长（进度条）
static uint8_t  s_presetIdx = 0;  // 当前预设
static uint16_t s_doneToday = 0;  // 今日完成数
static uint32_t s_doneDate = 0;   // 统计所属日期 (yyyyMMdd)
static int      s_focusMinutes = 0; // 进行中/刚完成的专注时长（SD 记录用）
static uint16_t s_minToday = 0;   // 今日累计专注分钟数

static const int kPresets[] = FOCUS_PRESETS;
static const int kNP = sizeof(kPresets) / sizeof(kPresets[0]);

// ================= 工具 =================
static void txt(Lcd& lcd, int16_t x, int16_t baselineY, const char* s, uint16_t fg) {
  s_u8.setFont(u8g2_font_wqy16_t_gb2312);
  s_u8.setForegroundColor(fg);
  s_u8.setBackgroundColor(C_BG);
  s_u8.setCursor(x, baselineY);
  s_u8.print(s);
}

static uint32_t todayKey() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return 0;
  return (uint32_t)(t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
}

static void fmtMMSS(char* buf, size_t n, uint32_t remainMs) {
  int sec = (int)((remainMs + 999) / 1000);   // 向上取整
  if (sec < 0) sec = 0;
  snprintf(buf, n, "%02d:%02d", sec / 60, sec % 60);
}

static void startFocus(int minutes) {
  s_focusMinutes = minutes;
  s_totalMs = (uint32_t)minutes * 60UL * 1000UL;
  s_endMs = millis() + s_totalMs;
  s_phase = ST_RUN_FOCUS;
  Serial.printf("[focus] start %d min\n", minutes);
}

static void sdLogSession() {
  if (!s_sdOk) return;
  struct tm t;
  if (!getLocalTime(&t, 50)) return;
  File f = SD_MMC.open("/focuslog.csv", FILE_APPEND);
  if (!f) { Serial.println("[sd] open focuslog.csv fail"); return; }
  if (f.size() == 0) f.println("date,time,minutes");
  f.printf("%04d-%02d-%02d,%02d:%02d:%02d,%d\n",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
           t.tm_hour, t.tm_min, t.tm_sec, s_focusMinutes);
  f.close();
  Serial.println("[sd] session logged");
}

static void startBreak() {
  bool longBreak = (s_doneToday > 0) && (s_doneToday % LONG_BREAK_EVERY == 0);
  int m = longBreak ? BREAK_LONG_MIN : BREAK_SHORT_MIN;
  s_totalMs = (uint32_t)m * 60UL * 1000UL;
  s_endMs = millis() + s_totalMs;
  s_phase = ST_RUN_BREAK;
  Serial.printf("[focus] break %s %d min (done=%u)\n", longBreak ? "long" : "short", m, s_doneToday);
}

static void saveStats() {
  Preferences prefs;
  prefs.begin("focus", false);
  prefs.putUShort("done", s_doneToday);
  prefs.putULong("date", s_doneDate);
  prefs.putUShort("mintoday", s_minToday);
  prefs.end();
}

// ================= 公开动作（按键与串口共用） =================
void Focus_ToggleStart() {
  switch (s_phase) {
    case ST_IDLE:        startFocus(kPresets[s_presetIdx]); s_flipPaused = false; break;
    case ST_RUN_FOCUS:   s_remainMs = s_endMs - millis(); s_phase = ST_PAUSE_FOCUS; s_flipPaused = false; break;
    case ST_PAUSE_FOCUS: s_endMs = millis() + s_remainMs; s_phase = ST_RUN_FOCUS;   s_flipPaused = false; break;
    case ST_RUN_BREAK:   s_remainMs = s_endMs - millis(); s_phase = ST_PAUSE_BREAK; s_flipPaused = false; break;
    case ST_PAUSE_BREAK: s_endMs = millis() + s_remainMs; s_phase = ST_RUN_BREAK;   s_flipPaused = false; break;
  }
}

void Focus_ResetOrPreset() {
  s_flipPaused = false;
  if (s_phase == ST_IDLE) {
    s_presetIdx = (s_presetIdx + 1) % kNP;   // 待机：切换预设时长
    Serial.printf("[focus] preset %d min\n", kPresets[s_presetIdx]);
  } else {
    s_phase = ST_IDLE;                        // 运行/暂停：重置回待机
    Serial.println("[focus] reset");
  }
}

void Focus_QuickTest() {
  if (s_phase == ST_IDLE) {
    s_totalMs = 10000;                        // 10 秒快速专注（验证流程）
    s_endMs = millis() + s_totalMs;
    s_phase = ST_RUN_FOCUS;
    Serial.println("[focus] QUICK TEST 10s");
  }
}

// ================= 渲染 =================
static void drawTomatoes(Lcd& lcd, int count) {
  int n = count > 8 ? 8 : count;
  for (int i = 0; i < n; i++) {
    int cx = 14 + i * 18, cy = 172;
    lcd.fillCircle(cx, cy, 6, C_RED);
    lcd.fillRect(cx - 2, cy - 9, 4, 3, C_GREEN);
  }
}

void Focus_Begin(Lcd& lcd) {
  s_u8.begin(lcd);
  lcd.fillRect(0, 0, LCD_W, LCD_H, C_BG);
  lcd.fillRect(0, 240, LCD_W, 1, C_HEAD_BG);

  Preferences prefs;
  prefs.begin("focus", false);   // 读写模式：首次使用会自动创建 namespace
  s_doneToday = prefs.getUShort("done", 0);
  s_doneDate  = prefs.getULong("date", 0);
  s_minToday  = prefs.getUShort("mintoday", 0);
  prefs.end();
  if (s_doneDate != todayKey()) { s_doneToday = 0; s_minToday = 0; }   // 跨天清零

  // 陀螺仪（翻面暂停）
  s_qmiOk = Qmi_Begin();
  Serial.printf("[focus] qmi %s\n", s_qmiOk ? "ok" : "not found");

  // TF 卡（专注记录）
  SD_MMC.setPins(14, 15, 16, 18, 17, 21);
  if (SD_MMC.begin("/sdcard", true, true) && SD_MMC.cardType() != CARD_NONE) {
    s_sdOk = true;
    Serial.printf("[focus] SD ok, %llu MB free\n",
                  (unsigned long long)((SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024)));
  } else {
    Serial.println("[focus] SD not present (记录不可用，其余功能正常)");
  }

  Serial.printf("[focus] init done=%u min=%u preset=%d\n", s_doneToday, s_minToday, kPresets[s_presetIdx]);
}

int Focus_RemainingSec() {
  bool running = (s_phase == ST_RUN_FOCUS || s_phase == ST_RUN_BREAK);
  uint32_t remainMs;
  if (running) remainMs = s_endMs - millis();
  else if (s_phase == ST_PAUSE_FOCUS || s_phase == ST_PAUSE_BREAK) remainMs = s_remainMs;
  else remainMs = (uint32_t)kPresets[s_presetIdx] * 60UL * 1000UL;
  return (int)((remainMs + 999) / 1000);
}

void Focus_Loop(Lcd& lcd) {
  // ---- 按键 ----
  static Btn btnStart(PIN_BTN_START), btnReset(PIN_BTN_RESET), btnSkip(PIN_BTN_SKIP);
  static bool btnInit = false;
  if (!btnInit) { btnStart.begin(); btnReset.begin(); btnSkip.begin(); btnInit = true; }
  if (btnStart.pressed()) Focus_ToggleStart();
  if (btnReset.pressed()) Focus_ResetOrPreset();
  if (btnSkip.pressed() && s_phase == ST_RUN_BREAK) {   // 跳过休息
    s_phase = ST_IDLE;
    s_presetIdx = 0;
    Serial.println("[focus] break skipped");
  }

  // ---- 陀螺仪翻面：倒计时中屏幕朝下 -> 暂停；翻回 -> 继续 ----
  static uint32_t lastSample = 0;
  static int flipStable = 0;
  if (s_qmiOk && millis() - lastSample >= 100) {
    lastSample = millis();
    float ax, ay, az;
    if (Qmi_GetAccel(ax, ay, az)) {
      bool down = (az < -0.6f);   // 屏幕朝下：z 为负约 -1g
      if (down != s_faceDown) {
        if (++flipStable >= 5) {  // 约 500ms 稳定才认定
          s_faceDown = down;
          flipStable = 0;
          Serial.printf("[focus] flip %s\n", down ? "down" : "up");
          bool running = (s_phase == ST_RUN_FOCUS || s_phase == ST_RUN_BREAK);
          if (down && running) {
            Focus_ToggleStart();
            s_flipPaused = true;
            Serial.println("[focus] flip -> paused");
          } else if (!down && s_flipPaused &&
                     (s_phase == ST_PAUSE_FOCUS || s_phase == ST_PAUSE_BREAK)) {
            Focus_ToggleStart();   // s_flipPaused 会被置 false
            Serial.println("[focus] flip -> resumed");
          }
        }
      } else {
        flipStable = 0;
      }
    }
  }

  // ---- 倒计时推进 ----
  if (s_phase == ST_RUN_FOCUS && (int32_t)(millis() - s_endMs) >= 0) {
    s_doneToday++;
    s_minToday += s_focusMinutes;
    s_doneDate = todayKey();
    saveStats();
    sdLogSession();
    Serial.printf("[focus] DONE! today=%u times, %u min\n", s_doneToday, s_minToday);
    startBreak();
  } else if (s_phase == ST_RUN_BREAK && (int32_t)(millis() - s_endMs) >= 0) {
    s_phase = ST_IDLE;
    s_presetIdx = 0;
    Serial.println("[focus] break over -> idle");
  }

  // ---- 变化驱动渲染 ----
  static Phase lastPhase = (Phase)-1;
  static int   lastPreset = -1;
  static int   lastSec = -1;
  static uint16_t lastDone = 0xFFFF;
  static int   lastPct = -1;

  bool running = (s_phase == ST_RUN_FOCUS || s_phase == ST_RUN_BREAK);
  uint32_t remainMs;
  if (running) remainMs = s_endMs - millis();
  else if (s_phase == ST_PAUSE_FOCUS || s_phase == ST_PAUSE_BREAK) remainMs = s_remainMs;
  else remainMs = (uint32_t)kPresets[s_presetIdx] * 60UL * 1000UL;

  int showSec = (int)((remainMs + 999) / 1000);
  int pct = s_totalMs ? (int)(100 - ((int64_t)remainMs * 100) / (int64_t)s_totalMs) : 0;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

  char buf[32];

  // 文本区：阶段/预设/统计变化时重绘
  if (s_phase != lastPhase || s_presetIdx != lastPreset || s_doneToday != lastDone) {
    lastPhase = s_phase; lastPreset = s_presetIdx; lastDone = s_doneToday;
    lcd.fillRect(0, 6, LCD_W, 20, C_BG);
    if (s_phase == ST_RUN_BREAK || s_phase == ST_PAUSE_BREAK)
      snprintf(buf, sizeof(buf), "休息一下");
    else
      snprintf(buf, sizeof(buf), "专注 %d 分钟", kPresets[s_presetIdx]);
    txt(lcd, 8, 22, buf, C_TEXT);

    lcd.fillRect(0, 130, LCD_W, 24, C_BG);
    const char* st = "待机";
    if (s_phase == ST_RUN_FOCUS)   st = "专注中";
    if (s_phase == ST_PAUSE_FOCUS) st = "已暂停";
    if (s_phase == ST_RUN_BREAK)   st = "休息中";
    if (s_phase == ST_PAUSE_BREAK) st = "休息暂停";
    txt(lcd, 8, 144, st, C_TEXT);

    lcd.fillRect(0, 150, LCD_W, 60, C_BG);
    drawTomatoes(lcd, s_doneToday);
    snprintf(buf, sizeof(buf), "今日完成 %u 次 · %u 分钟", s_doneToday, s_minToday);
    txt(lcd, 8, 205, buf, C_TEXT);

    lcd.fillRect(0, 246, LCD_W, 70, C_BG);
    txt(lcd, 8, 272, "11 开始 / 暂停", C_DIM);
    txt(lcd, 8, 296, "10 重置 / 预设   9 跳过休息", C_DIM);
  }

  // 大倒计时：秒变化时重绘
  if (showSec != lastSec) {
    lastSec = showSec;
    fmtMMSS(buf, sizeof(buf), remainMs);
    lcd.fillRect(0, 30, LCD_W, 50, C_BG);
    s_u8.setFont(u8g2_font_logisoso42_tr);
    int16_t w = s_u8.getUTF8Width(buf);
    s_u8.setForegroundColor(C_TEXT);
    s_u8.setBackgroundColor(C_BG);
    s_u8.setCursor((LCD_W - w) / 2, 78);
    s_u8.print(buf);
  }

  // 进度条：百分比变化时重绘
  if (pct != lastPct) {
    lastPct = pct;
    uint16_t color = C_GREEN;
    if (s_phase == ST_RUN_BREAK || s_phase == ST_PAUSE_BREAK) color = C_CYAN;
    if (s_phase == ST_PAUSE_FOCUS || s_phase == ST_PAUSE_BREAK) color = C_ORANGE;
    if (s_phase == ST_IDLE) color = C_HEAD_BG;
    lcd.fillRect(4, 104, 164, 10, C_BAR_BG);
    if (pct > 0) lcd.fillRect(4, 104, (int)(164 * pct / 100), 10, color);
  }
}
