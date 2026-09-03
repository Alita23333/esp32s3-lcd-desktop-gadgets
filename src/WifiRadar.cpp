#include "WifiRadar.hpp"
#include "Config.h"
#include <WiFi.h>
#include <U8g2_for_Adafruit_GFX.h>

static U8G2_FOR_ADAFRUIT_GFX s_u8;
static Lcd* s_lcd = nullptr;

// ================= 雷达几何 =================
static const int16_t CX = 86, CY = 130, CR = 78;

// ================= 网络数据 =================
#define MAX_NETS 30
struct Net {
  char    ssid[33];
  int32_t rssi;
  uint8_t ch;
  bool    enc;
  uint16_t ang;      // 雷达上的角度（按 SSID 哈希）
};
static Net  s_nets[MAX_NETS];
static int  s_count = 0;        // 本轮发现的网络总数
static int  s_shown = 0;        // 列表显示数
static bool s_scanning = false;
static uint32_t s_scanDoneAt = 0;
static uint32_t s_scanStartedAt = 0;
static uint32_t s_scanCount = 0;

static float s_sweep = 0;       // 扫描线角度
static uint32_t s_lastSweep = 0;

static void drawHeader();
static void drawList();

static void txt(int16_t x, int16_t baselineY, const char* s, uint16_t fg, uint16_t bg) {
  s_u8.setFont(u8g2_font_wqy16_t_gb2312);
  s_u8.setForegroundColor(fg);
  s_u8.setBackgroundColor(bg);
  s_u8.setCursor(x, baselineY);
  s_u8.print(s);
}

static uint16_t hashAngle(const char* s) {
  uint32_t h = 0;
  for (const char* p = s; *p; p++) h = h * 137 + (uint8_t)*p;
  return (uint16_t)(h % 360);
}

// ================= 扫描 =================
// 注意：同步 scanNetworks 第二次调用会挂起；异步 scanComplete 在启动后立刻读取会返回
// 旧结果数(假 0)。因此用异步 + 时间门控：启动 2.5 秒后才允许读结果，超时自动重试。
static void startScan() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true);   // 异步
  s_scanning = true;
  s_scanStartedAt = millis();
  Serial.println("[radar] scan start");
}

static void collectResults(int n) {
  s_scanning = false;
  if (n <= 0 && s_count > 0) {
    // 偶发空结果：保留上一轮（宁有旧数据不闪空屏），下轮再试
    Serial.println("[radar] scan empty, keep previous");
    s_scanDoneAt = millis();
    return;
  }
  s_count = (n > 0 && n > MAX_NETS) ? MAX_NETS : (n > 0 ? n : 0);
  for (int i = 0; i < s_count; i++) {
    Net& net = s_nets[i];
    // SSID 来自空中广播，属不可信输入：snprintf 保证边界与终止符
    snprintf(net.ssid, sizeof(net.ssid), "%s", WiFi.SSID(i).c_str());
    if (!net.ssid[0]) snprintf(net.ssid, sizeof(net.ssid), "(hidden)");
    net.rssi = WiFi.RSSI(i);
    net.ch   = WiFi.channel(i);
    net.enc  = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    net.ang  = hashAngle(net.ssid);
  }
  // 按信号强度降序排序（简单冒泡，n<=30）
  for (int i = 0; i < s_count - 1; i++)
    for (int j = 0; j < s_count - 1 - i; j++)
      if (s_nets[j].rssi < s_nets[j + 1].rssi) {
        Net tmp = s_nets[j]; s_nets[j] = s_nets[j + 1]; s_nets[j + 1] = tmp;
      }
  s_shown = s_count > 4 ? 4 : s_count;
  WiFi.scanDelete();          // 结果已拷贝到 s_nets，释放扫描缓冲
  s_scanCount++;
  s_scanDoneAt = millis();
  Serial.printf("[radar] scan #%u: %d networks\n", s_scanCount, s_count);
  drawHeader();
  drawList();
}

// ================= 渲染 =================
static void drawHeader() {
  char buf[32];
  s_lcd->fillRect(0, 0, LCD_W, 26, C_BG);
  if (s_scanning) {
    snprintf(buf, sizeof(buf), "RADAR 扫描中...");
  } else {
    snprintf(buf, sizeof(buf), "RADAR:%d", s_count);
  }
  txt(6, 19, buf, C_TEXT, C_BG);
}

static void drawRadar() {
  s_lcd->fillRect(0, 28, LCD_W, 190, C_BG);
  // 外环 / 内环 / 十字线
  s_lcd->drawCircle(CX, CY, CR, C_HEAD_BG);
  s_lcd->drawCircle(CX, CY, CR / 2, C_HEAD_BG);
  s_lcd->drawLine(CX - CR, CY, CX + CR, CY, C_HEAD_BG);
  s_lcd->drawLine(CX, CY - CR, CX, CY + CR, C_HEAD_BG);

  // 扫描线 + 渐隐尾迹
  for (int t = 3; t >= 0; t--) {
    float a = (s_sweep - t * 9) * DEG_TO_RAD;
    uint16_t c = (t == 0) ? C_GREEN : C_HEAD_BG;
    s_lcd->drawLine(CX, CY,
                    CX + (int)(cosf(a) * CR), CY + (int)(sinf(a) * CR), c);
  }

  // 网络亮点：角度=哈希，半径=信号强度；扫过时高亮
  for (int i = 0; i < s_count; i++) {
    float a = s_nets[i].ang * DEG_TO_RAD;
    int16_t r = 20 + (int)((s_nets[i].rssi + 100) * (CR - 26) / 70);   // -100..-30 -> 20..CR-26
    if (r < 14) r = 14;
    if (r > CR - 8) r = CR - 8;
    int16_t bx = CX + (int)(cosf(a) * r);
    int16_t by = CY + (int)(sinf(a) * r);
    float diff = fabsf(s_sweep - s_nets[i].ang);
    if (diff > 180) diff = 360 - diff;
    bool lit = diff < 20;
    s_lcd->fillCircle(bx, by, lit ? 4 : 2, lit ? C_GREEN : C_DIM);
  }
}

static void drawList() {
  s_lcd->fillRect(0, 222, LCD_W, 96, C_BG);
  char buf[48];
  for (int i = 0; i < s_shown; i++) {
    int16_t y = 232 + i * 22;
    // 信号条（宽度按 rssi：-30→140px，-100→20px）
    int w = 20 + (int)((s_nets[i].rssi + 100) * 120 / 70);
    if (w < 10) w = 10;
    if (w > 140) w = 140;
    s_lcd->fillRect(6, y - 6, w, 6, s_nets[i].rssi > -60 ? C_GREEN : (s_nets[i].rssi > -75 ? C_ORANGE : C_RED));
    // 名称 + 信道
    snprintf(buf, sizeof(buf), "%.10s ch%d %d", s_nets[i].ssid, s_nets[i].ch, (int)s_nets[i].rssi);
    txt(6, y + 10, buf, C_TEXT, C_BG);
  }
  if (s_count == 0 && !s_scanning) {
    txt((LCD_W - 96) / 2, 250, "未发现网络", C_DIM, C_BG);
  }
}

// ================= 公开接口 =================
void Radar_Begin(Lcd& lcd) {
  s_lcd = &lcd;
  s_u8.begin(lcd);
  lcd.fillRect(0, 0, LCD_W, LCD_H, C_BG);
  drawHeader();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);   // 未连接状态下的重复扫描也需射频保持活跃
  startScan();

  Serial.println("[radar] begin, scanning...");
}

void Radar_Loop(Lcd& lcd) {
  // 异步扫描 + 时间门控：启动 2.5 秒后才读结果（避开假 0），10 秒超时重试
  if (s_scanning) {
    uint32_t elapsed = millis() - s_scanStartedAt;
    if (elapsed > 2500) {
      int r = WiFi.scanComplete();
      if (r >= 0) {
        collectResults(r);
      } else if (r == WIFI_SCAN_FAILED || elapsed > 10000) {
        Serial.println("[radar] scan failed, retry");
        s_scanning = false;
        s_scanDoneAt = millis();
      }
      // r == -1 仍在运行，继续等
    }
  } else if (millis() - s_scanDoneAt > RADAR_RESCAN_MS) {
    startScan();
  }

  // 扫描线动画（约 30fps）
  if (millis() - s_lastSweep >= 33) {
    s_lastSweep = millis();
    s_sweep += 8;
    if (s_sweep >= 360) s_sweep -= 360;
    drawRadar();
  }
}

void Radar_ForceScan() {
  startScan();
}

void Radar_PrintTelemetry() {
  for (int i = 0; i < s_count; i++) {
    Serial.printf("#W,%d,%s,%d,%d,%d\n", i, s_nets[i].ssid, (int)s_nets[i].rssi,
                  s_nets[i].ch, s_nets[i].enc ? 1 : 0);
  }
  Serial.printf("#WEND,%d,%u\n", s_count, s_scanCount);
}
