#include "Game2048.hpp"
#include "Config.h"
#include "Button.hpp"
#include "Qmi.hpp"
#include <Preferences.h>
#include <U8g2_for_Adafruit_GFX.h>

static U8G2_FOR_ADAFRUIT_GFX s_u8;
static Lcd* s_lcd = nullptr;

// ================= 棋盘几何 =================
#define T_CELL 38
#define T_GAP  4
#define T_OX   0                     // 4*38+5*4 = 172 恰好满宽
#define T_OY   44

// ================= 状态 =================
static int  s_b[4][4];               // 0=空，否则为面值(2/4/.../2048)
static uint32_t s_score = 0;
static uint16_t s_best = 0;
static bool s_over = false;
static bool s_winShown = false;
  static Btn s_btnUp(PIN_BTN_START), s_btnDown(PIN_BTN_RESET), s_btnLeft(PIN_BTN_SKIP), s_btnRight(PIN_BTN_RIGHT);
static bool s_btnInit = false;

static bool s_qmiOk = false;
static float s_nx = 0, s_ny = 0;        // 中立姿态
static int   s_lastZone = 0;            // 上次倾斜分区（0=中位）
static uint32_t s_lastZoneMs = 0;

// ================= 工具 =================
static uint16_t rgb565(int r, int g, int b) {
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

// 经典 2048 配色（RGB888）
static uint16_t tileColor(int v, bool bg) {
  struct C { int v; int r, g, b; };
  static const C tab[] = {
    {2,    238,228,218}, {4,    237,224,200}, {8,    242,177,121},
    {16,   245,149, 99}, {32,   246,124, 95}, {64,   246, 94, 59},
    {128,  237,207,114}, {256,  237,204, 97}, {512,  237,200, 80},
    {1024, 237,197, 63}, {2048, 237,194, 46},
  };
  if (v == 0) return C_BAR_BG;
  for (auto& c : tab)
    if (c.v == v) return bg ? rgb565(c.r, c.g, c.b) : rgb565(255, 255, 255);
  return bg ? rgb565(60, 40, 60) : C_TEXT;   // 超过 2048 的紫色
}

static void txt(int16_t x, int16_t baselineY, const char* s, uint16_t fg, uint16_t bg) {
  s_u8.setFont(u8g2_font_wqy16_t_gb2312);
  s_u8.setForegroundColor(fg);
  s_u8.setBackgroundColor(bg);
  s_u8.setCursor(x, baselineY);
  s_u8.print(s);
}

// ================= 核心逻辑 =================
static void spawn() {
  int empty[16], n = 0;
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      if (s_b[r][c] == 0) empty[n++] = r * 4 + c;
  if (n == 0) return;
  int idx = empty[random(n)];
  s_b[idx / 4][idx % 4] = (random(100) < 90) ? 2 : 4;
}

static bool canMove() {
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++) {
      if (s_b[r][c] == 0) return true;
      if (c < 3 && s_b[r][c] == s_b[r][c + 1]) return true;
      if (r < 3 && s_b[r][c] == s_b[r + 1][c]) return true;
    }
  return false;
}

// 对长度 n 的一维数组向左滑动合并，返回是否变化
static bool slide(int* line, int n, uint32_t& gain) {
  int tmp[4], t = 0;
  for (int i = 0; i < n; i++)
    if (line[i]) tmp[t++] = line[i];
  int out[4], o = 0;
  for (int i = 0; i < t; i++) {
    if (i + 1 < t && tmp[i] == tmp[i + 1]) {
      out[o++] = tmp[i] * 2;
      gain += tmp[i] * 2;
      i++;
    } else {
      out[o++] = tmp[i];
    }
  }
  while (o < n) out[o++] = 0;
  bool moved = false;
  for (int i = 0; i < n; i++) {
    if (line[i] != out[i]) moved = true;
    line[i] = out[i];
  }
  return moved;
}

// dir: 0=左 1=右 2=上 3=下
static bool move(int dir) {
  uint32_t gain = 0;
  bool moved = false;
  for (int k = 0; k < 4; k++) {
    int line[4];
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        int r = (dir == 0 || dir == 1) ? k : (dir == 2 ? j : 3 - j);
        int c = (dir == 0 || dir == 1) ? (dir == 0 ? j : 3 - j) : k;
        line[j] = s_b[r][c];
      }
      if (slide(line, 4, gain)) moved = true;
      for (int j = 0; j < 4; j++) {
        int r = (dir == 0 || dir == 1) ? k : (dir == 2 ? j : 3 - j);
        int c = (dir == 0 || dir == 1) ? (dir == 0 ? j : 3 - j) : k;
        s_b[r][c] = line[j];
      }
    }
  }
  if (moved) {
    s_score += gain;
    spawn();
    if (!s_winShown) {
      bool win = false;
      for (int r = 0; r < 4 && !win; r++)
        for (int c = 0; c < 4; c++)
          if (s_b[r][c] >= 2048) win = true;
      if (win) s_winShown = true;
    }
    if (s_score > s_best) {
      s_best = s_score;
      Preferences prefs;
      prefs.begin("g2048", false);
      prefs.putUShort("best", (uint16_t)(s_best > 65535 ? 65535 : s_best));
      prefs.end();
    }
  }
  return moved;
}

static void newGame() {
  memset(s_b, 0, sizeof(s_b));
  s_score = 0;
  s_over = false;
  s_winShown = false;
  spawn();
  spawn();
  Serial.println("[2048] new game");
}

// ================= 渲染 =================
static void drawBoard() {
  char buf[8];
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++) {
      int16_t x = T_OX + T_GAP + c * (T_CELL + T_GAP);
      int16_t y = T_OY + T_GAP + r * (T_CELL + T_GAP);
      int v = s_b[r][c];
      s_lcd->fillRect(x, y, T_CELL, T_CELL, tileColor(v, true));
      if (v == 0) continue;
      snprintf(buf, sizeof(buf), "%d", v);
      uint8_t len = strlen(buf);
      uint8_t size = (len <= 2) ? 3 : (len == 3 ? 2 : 1);
      s_lcd->setTextSize(size);   // GFX 内置字体，与 u8g2 适配器互不干扰
      s_lcd->setTextColor(len >= 3 ? C_TEXT : 0x5A5A, tileColor(v, true));
      int16_t w = len * 6 * size, h = 8 * size;
      s_lcd->setCursor(x + (T_CELL - w) / 2, y + (T_CELL - h) / 2);
      s_lcd->print(buf);
    }
  s_lcd->setTextSize(1);
}

static void drawAll() {
  drawBoard();
  char buf[32];
  s_lcd->fillRect(0, 0, LCD_W, T_OY - 4, C_BG);
  snprintf(buf, sizeof(buf), "SCORE %u", (unsigned)s_score);
  txt(6, 20, buf, C_TEXT, C_BG);
  snprintf(buf, sizeof(buf), "BEST %u", s_best);
  int rx = LCD_W - 6 - strlen(buf) * 6;
  s_lcd->setTextSize(1);
  s_lcd->setTextColor(C_DIM, C_BG);
  s_lcd->setCursor(rx, 12);
  s_lcd->print(buf);

  s_lcd->fillRect(0, 196, LCD_W, 24, C_BG);
  if (s_over) {
    txt((LCD_W - 64) / 2, 212, "游戏结束", C_TEXT, C_BG);
    txt((LCD_W - 120) / 2, 236, "GP11 长按重开", C_DIM, C_BG);
  } else if (s_winShown) {
    txt((LCD_W - 112) / 2, 212, "到达 2048！继续？", C_TEXT, C_BG);
  } else {
    txt(6, 212, "倾斜 / WASD 移动", C_TEXT, C_BG);
    txt(6, 236, "GP11上 GP10下 GP9左 GP8右", C_DIM, C_BG);
  }
}

// ================= 公开接口 =================
void G2048_Begin(Lcd& lcd) {
  s_lcd = &lcd;
  s_u8.begin(lcd);

  Preferences prefs;
  prefs.begin("g2048", false);
  s_best = prefs.getUShort("best", 0);
  prefs.end();

  s_qmiOk = Qmi_Begin();
  // 标定中立姿态（开始时板子的摆放角度）
  float ax = 0, ay = 0, x, y, z;
  int n = 10;
  for (int i = 0; i < n; i++) {
    if (Qmi_GetAccel(x, y, z)) { ax += x; ay += y; }
    delay(20);
  }
  s_nx = ax / n; s_ny = ay / n;
  Serial.printf("[2048] gyro %s neutral %.2f %.2f\n", s_qmiOk ? "ok" : "x", s_nx, s_ny);

  s_lcd->fillRect(0, 0, LCD_W, LCD_H, C_BG);
  newGame();
  drawAll();
}

void G2048_Input(char c) {
  bool moved = false;
  switch (c) {
    case 'w': case 'k': moved = move(2); break;
    case 'a': case 'h': moved = move(0); break;
    case 's': case 'j': moved = move(3); break;
    case 'd': case 'l': moved = move(1); break;
    case 'g':
      newGame();
      drawAll();
      return;
    case 'p': {
      Serial.printf("#B,%u,%u,%d", (unsigned)s_score, s_best, s_over ? 1 : 0);
      for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
          Serial.printf(",%d", s_b[r][c]);
      Serial.println();
      return;
    }
  }
  if (moved) {
    drawAll();
    if (!canMove()) {
      s_over = true;
      drawAll();
      Serial.printf("[2048] game over score=%u\n", (unsigned)s_score);
    }
  }
}

void G2048_Loop(Lcd& lcd) {
  if (!s_btnInit) {
    s_btnUp.begin(); s_btnDown.begin(); s_btnLeft.begin(); s_btnRight.begin();
    s_btnInit = true;
  }
  if (s_btnUp.pressed())    G2048_Input('w');
  if (s_btnDown.pressed())  G2048_Input('s');
  if (s_btnLeft.pressed())  G2048_Input('a');
  if (s_btnRight.pressed()) G2048_Input('d');

  // 游戏结束后 GP11 长按 1 秒重开
  static uint32_t holdStart = 0;
  if (s_over && s_btnUp.isDown()) {
    if (holdStart == 0) holdStart = millis();
    if (millis() - holdStart > 1000) { holdStart = 0; G2048_Input('g'); }
  } else {
    holdStart = 0;
  }

  // 倾斜分区（边沿触发）：偏离中立位的主导轴 -> 一次移动
  if (s_qmiOk && !s_over) {
    static uint32_t lastSample = 0;
    if (millis() - lastSample >= 80) {
      lastSample = millis();
      float x, y, z;
      if (Qmi_GetAccel(x, y, z)) {
        float rx = x - s_nx, ry = y - s_ny;
        int zone = 0;
        if (fabsf(rx) > 0.45f && fabsf(rx) > fabsf(ry)) zone = rx > 0 ? 1 : 2;   // 右/左
        else if (fabsf(ry) > 0.45f)                    zone = ry > 0 ? 3 : 4;   // 下/上
        if (zone != s_lastZone && millis() - s_lastZoneMs > 250) {
          s_lastZone = zone;
          s_lastZoneMs = millis();
          if (zone == 1) G2048_Input('d');
          else if (zone == 2) G2048_Input('a');
          else if (zone == 3) G2048_Input('s');
          else if (zone == 4) G2048_Input('w');
        }
      }
    }
  }
}
