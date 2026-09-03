#include "Snake.hpp"
#include "Config.h"
#include "Button.hpp"
#include "Qmi.hpp"
#include <Preferences.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <string.h>
#include <math.h>

static U8G2_FOR_ADAFRUIT_GFX s_u8;

// ================= 棋盘 =================
#define CELL 10
#define COLS 17
#define ROWS 29
#define OX   ((LCD_W - COLS * CELL) / 2)
#define OY   24

// ================= 状态 =================
enum GState { G_READY, G_RUN, G_PAUSE, G_OVER };
static GState s_state = G_READY;

static int16_t s_x[COLS * ROWS], s_y[COLS * ROWS];   // [0]=头
static int16_t s_len = 3;
static int8_t  s_dx = 1, s_dy = 0;                   // 当前方向
static int8_t  s_pdx = 1, s_pdy = 0;                 // 待应用方向（防一帧内掉头）
static int16_t s_fx = -1, s_fy = -1;
static uint16_t s_score = 0, s_hi = 0;
static uint32_t s_lastTick = 0;
static uint16_t s_tickMs = 160;

static Btn s_btnA(PIN_BTN_START), s_btnB(PIN_BTN_RESET);
static bool s_btnInit = false;

static bool s_qmiOk = false;
static float s_nx = 0, s_ny = 0;                     // 中立姿态（开始时标定）

static Lcd* s_lcd = nullptr;

// ================= 工具 =================
static void drawCell(int16_t cx, int16_t cy, uint16_t color) {
  s_lcd->fillRect(OX + cx * CELL + 1, OY + cy * CELL + 1, CELL - 2, CELL - 2, color);
}

static bool cellOnSnake(int16_t cx, int16_t cy) {
  for (int16_t i = 0; i < s_len; i++)
    if (s_x[i] == cx && s_y[i] == cy) return true;
  return false;
}

static void placeFood() {
  do {
    s_fx = random(COLS);
    s_fy = random(ROWS);
  } while (cellOnSnake(s_fx, s_fy));
  drawCell(s_fx, s_fy, C_RED);
}

static void txt_label(Lcd* lcd, int16_t x, int16_t baselineY, const char* s) {
  s_u8.setFont(u8g2_font_wqy16_t_gb2312);
  s_u8.setForegroundColor(C_TEXT);
  s_u8.setBackgroundColor(C_BG);
  s_u8.setCursor(x, baselineY);
  s_u8.print(s);
}

static void drawHeader() {
  char buf[24];
  s_lcd->fillRect(0, 0, LCD_W, OY, C_BG);
  snprintf(buf, sizeof(buf), "S:%u HI:%u", s_score, s_hi);
  txt_label(s_lcd, 6, 17, buf);
}

// ================= 流程 =================
static void calibrateNeutral() {
  float ax = 0, ay = 0, az = 0, x, y, z;
  int n = 10;
  for (int i = 0; i < n; i++) {
    if (Qmi_GetAccel(x, y, z)) { ax += x; ay += y; az += z; }
    delay(20);
  }
  s_nx = ax / n; s_ny = ay / n;
  Serial.printf("[snake] neutral %.2f %.2f\n", s_nx, s_ny);
}

static void restart() {
  s_len = 3;
  for (int16_t i = 0; i < s_len; i++) { s_x[i] = 8 - i; s_y[i] = ROWS / 2; }
  s_dx = 1; s_dy = 0; s_pdx = 1; s_pdy = 0;
  s_score = 0;
  s_tickMs = 160;
  s_state = G_RUN;
  s_lcd->fillRect(0, OY, LCD_W, LCD_H - OY, C_BG);
  drawHeader();
  // 画初始蛇身
  for (int16_t i = 0; i < s_len; i++) drawCell(s_x[i], s_y[i], i == 0 ? C_TEXT : C_GREEN);
  calibrateNeutral();
  placeFood();
  s_lastTick = millis();
  Serial.println("[snake] start");
}

static void gameOver() {
  s_state = G_OVER;
  if (s_score > s_hi) {
    s_hi = s_score;
    Preferences prefs;
    prefs.begin("snake", false);
    prefs.putUShort("hi", s_hi);
    prefs.end();
  }
  Serial.printf("[snake] game over score=%u hi=%u\n", s_score, s_hi);
  // 结果覆盖层
  s_lcd->fillRect(16, 110, LCD_W - 32, 100, C_HEAD_BG);
  char buf[24];
  s_u8.setFont(u8g2_font_wqy16_t_gb2312);
  s_u8.setForegroundColor(C_TEXT);
  s_u8.setBackgroundColor(C_HEAD_BG);
  s_u8.setCursor(40, 140);  s_u8.print("游戏结束");
  snprintf(buf, sizeof(buf), "得分 %u  最高 %u", s_score, s_hi);
  s_u8.setCursor(32, 168);  s_u8.print(buf);
  s_u8.setCursor(32, 194);  s_u8.print("GP11 重开");
}

static void tick() {
  s_dx = s_pdx; s_dy = s_pdy;
  int16_t nhx = s_x[0] + s_dx, nhy = s_y[0] + s_dy;

  // 撞墙
  if (nhx < 0 || nhy < 0 || nhx >= COLS || nhy >= ROWS) { gameOver(); return; }

  bool grow = (nhx == s_fx && nhy == s_fy);
  if (grow && s_len >= COLS * ROWS) { gameOver(); return; }   // 满盘即通关

  // 撞自己（不增长时尾巴会腾出，可排除尾格）
  int16_t checkLen = s_len - (grow ? 0 : 1);
  for (int16_t i = 0; i < checkLen; i++)
    if (s_x[i] == nhx && s_y[i] == nhy) { gameOver(); return; }

  memmove(&s_x[1], &s_x[0], s_len * sizeof(int16_t));
  memmove(&s_y[1], &s_y[0], s_len * sizeof(int16_t));
  s_x[0] = nhx; s_y[0] = nhy;

  if (grow) {
    s_len++;   // 移位后头部占 0..s_len，长度 +1
    s_score++;
    if (s_tickMs > 90) s_tickMs -= 3;
    drawCell(nhx, nhy, C_TEXT);
    drawHeader();
    Serial.printf("[snake] food! score=%u len=%d\n", s_score, s_len);
    placeFood();
  } else {
    // 移位后旧尾格位于下标 s_len，擦除即可；长度不变（此处曾误写 s_len-- 导致负长度崩溃）
    drawCell(s_x[s_len], s_y[s_len], C_BG);
    drawCell(nhx, nhy, C_TEXT);
  }
}

// ================= 转向 =================
static void setDir(int8_t nx, int8_t ny) {
  if (nx == 0 && ny == 0) return;
  if (nx == -s_pdx && ny == -s_pdy) return;   // 禁止 180 度掉头
  s_pdx = nx; s_pdy = ny;
}

void Snake_Input(char c) {
  switch (c) {
    case 'w': setDir(0, -1); break;
    case 'a': setDir(-1, 0); break;
    case 's': setDir(0, 1);  break;
    case 'd': setDir(1, 0);  break;
    case 'g': if (s_state != G_RUN) restart(); break;
    case 'p': {
      float ax, ay, az;
      Qmi_GetAccel(ax, ay, az);
      Serial.printf("#S,%u,%d,%d,%d,%d,%d,%d,%d,%d,%.2f,%.2f,%.2f\n",
                    s_state, s_score, s_len, s_x[0], s_y[0], s_fx, s_fy,
                    s_dx, s_dy, ax, ay, az);
      break;
    }
  }
}

// ================= 公开接口 =================
void Snake_Begin(Lcd& lcd) {
  s_lcd = &lcd;
  s_u8.begin(lcd);
  lcd.fillRect(0, 0, LCD_W, LCD_H, C_BG);
  drawHeader();

  s_qmiOk = Qmi_Begin();
  Serial.printf("[snake] gyro %s\n", s_qmiOk ? "ok" : "not found");

  Preferences prefs;
  prefs.begin("snake", false);   // 读写模式：首次自动创建 namespace
  s_hi = prefs.getUShort("hi", 0);
  prefs.end();

  // 标题界面
  s_u8.setFont(u8g2_font_logisoso42_tr);
  s_u8.setForegroundColor(C_GREEN);
  s_u8.setBackgroundColor(C_BG);
  s_u8.setCursor(38, 130);
  s_u8.print("SNAKE");
  char buf[24];
  snprintf(buf, sizeof(buf), "最高分 %u", s_hi);
  txt_label(s_lcd, 46, 170, buf);
  txt_label(s_lcd, 22, 210, "倾斜板子控制方向");
  txt_label(s_lcd, 22, 236, "串口 WASD 也可以");
  txt_label(s_lcd, 40, 272, "GP11 开始");

  Serial.printf("[snake] ready hi=%u\n", s_hi);
}

void Snake_TogglePause() {
  if (s_state == G_RUN) {
    s_state = G_PAUSE;
    txt_label(s_lcd, 56, 160, "已暂停");
  } else if (s_state == G_PAUSE) {
    s_state = G_RUN;
    s_lcd->fillRect(0, OY, LCD_W, LCD_H - OY, C_BG);
    drawHeader();
    for (int16_t i = 0; i < s_len; i++) drawCell(s_x[i], s_y[i], i == 0 ? C_TEXT : C_GREEN);
    drawCell(s_fx, s_fy, C_RED);
  }
}

void Snake_Loop(Lcd& lcd) {
  if (!s_btnInit) { s_btnA.begin(); s_btnB.begin(); s_btnInit = true; }

  if (s_btnA.pressed() && (s_state == G_READY || s_state == G_OVER)) restart();
  if (s_btnB.pressed()) Snake_TogglePause();

  if (s_state != G_RUN) return;

  // 陀螺仪转向
  if (s_qmiOk) {
    float ax, ay, az;
    if (Qmi_GetAccel(ax, ay, az)) {
      float rx = ax - s_nx, ry = ay - s_ny;
      if (fabsf(rx) > 0.25f || fabsf(ry) > 0.25f) {
        if (fabsf(rx) > fabsf(ry)) setDir((int8_t)(rx > 0 ? 1 : -1), 0);
        else                       setDir(0, (int8_t)(ry > 0 ? 1 : -1));
      }
    }
  }

  // 游戏节拍
  if (millis() - s_lastTick >= s_tickMs) {
    s_lastTick = millis();
    tick();
  }
}
