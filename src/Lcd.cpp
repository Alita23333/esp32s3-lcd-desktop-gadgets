#include "Lcd.hpp"
#include "Config.h"
#include <SPI.h>

// ---- 引脚（Waveshare ESP32-S3-LCD-1.47B）----
#define PIN_LCD_MOSI 45
#define PIN_LCD_SCLK 40
#define PIN_LCD_CS   42
#define PIN_LCD_DC   41
#define PIN_LCD_RST  39
#define PIN_LCD_BL   46

// ST7789 GRAM 宽 240，面板可见区仅 172 宽 -> 列偏移 34，否则右侧花屏
#define OFFSET_X 34

static SPIClass* s_spi = nullptr;
static uint8_t s_blChannel = 0;

// 设置背光占空比（0-100）
void Lcd::setBacklight(uint8_t dutyPercent) {
  if (dutyPercent > 100) dutyPercent = 100;
  ledcWrite(s_blChannel, 1023 * dutyPercent / 100);
}

static inline void wrCmd(uint8_t c) {
  digitalWrite(PIN_LCD_DC, LOW);
  digitalWrite(PIN_LCD_CS, LOW);
  s_spi->write(c);
  digitalWrite(PIN_LCD_CS, HIGH);
}

static inline void wrDat(uint8_t d) {
  digitalWrite(PIN_LCD_DC, HIGH);
  digitalWrite(PIN_LCD_CS, LOW);
  s_spi->write(d);
  digitalWrite(PIN_LCD_CS, HIGH);
}

void Lcd::begin() {
  pinMode(PIN_LCD_CS, OUTPUT);
  pinMode(PIN_LCD_DC, OUTPUT);
  pinMode(PIN_LCD_RST, OUTPUT);

  // 背光 PWM 调光（初始用夜间低亮度，时间同步后由日历 UI 自适应切换）
  // 本框架为旧版通道制 LEDC API（ledcSetup/ledcAttachPin/ledcWrite(channel)）
  ledcSetup(s_blChannel, 1000, 10);
  ledcAttachPin(PIN_LCD_BL, s_blChannel);
  ledcWrite(s_blChannel, 1023 * BACKLIGHT_NIGHT / 100);

  s_spi = &SPI;
  s_spi->begin(PIN_LCD_SCLK, -1, PIN_LCD_MOSI, -1);
  s_spi->setFrequency(80000000);
  s_spi->setDataMode(SPI_MODE0);

  // 硬复位
  digitalWrite(PIN_LCD_RST, HIGH); delay(50);
  digitalWrite(PIN_LCD_RST, LOW);  delay(50);
  digitalWrite(PIN_LCD_RST, HIGH); delay(120);

  // ---- WaveShare 官方初始化序列（像素格式改为 0x55 RGB565 以适配 GFX）----
  wrCmd(0x11); delay(120);            // Sleep Out
  wrCmd(0x36); wrDat(0x00);           // MADCTL: 竖屏
  wrCmd(0x3A); wrDat(0x55);           // COLMOD: RGB565
  wrCmd(0xB0); wrDat(0x00); wrDat(0xE8);
  wrCmd(0xB2); wrDat(0x0C); wrDat(0x0C); wrDat(0x00); wrDat(0x33); wrDat(0x33);
  wrCmd(0xB7); wrDat(0x35);           // Gate 电压
  wrCmd(0xBB); wrDat(0x35);           // VCOM
  wrCmd(0xC0); wrDat(0x2C);
  wrCmd(0xC2); wrDat(0x01);
  wrCmd(0xC3); wrDat(0x13);
  wrCmd(0xC4); wrDat(0x20);
  wrCmd(0xC6); wrDat(0x0F);
  wrCmd(0xD0); wrDat(0xA4); wrDat(0xA1);
  wrCmd(0xD6); wrDat(0xA1);
  wrCmd(0xE0);                        // Gamma 正
  wrDat(0xF0); wrDat(0x00); wrDat(0x04); wrDat(0x04); wrDat(0x04); wrDat(0x05);
  wrDat(0x29); wrDat(0x33); wrDat(0x3E); wrDat(0x38); wrDat(0x12); wrDat(0x12);
  wrDat(0x28); wrDat(0x30);
  wrCmd(0xE1);                        // Gamma 负
  wrDat(0xF0); wrDat(0x07); wrDat(0x0A); wrDat(0x0D); wrDat(0x0B); wrDat(0x07);
  wrDat(0x28); wrDat(0x33); wrDat(0x3E); wrDat(0x36); wrDat(0x14); wrDat(0x14);
  wrDat(0x29); wrDat(0x32);
  wrCmd(0x21);                        // 反色开（该面板需要）
  wrCmd(0x11); delay(120);            // 再 Sleep Out 一次（与官方一致）
  wrCmd(0x29);                        // Display On
}

// ---- 地址窗口（含列偏移）----
void Lcd::setWindow(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  uint16_t xs = (uint16_t)x0 + OFFSET_X;
  uint16_t xe = (uint16_t)x1 + OFFSET_X;
  wrCmd(0x2A);
  wrDat(xs >> 8); wrDat(xs & 0xFF); wrDat(xe >> 8); wrDat(xe & 0xFF);
  wrCmd(0x2B);
  wrDat(((uint16_t)y0) >> 8); wrDat((uint16_t)y0 & 0xFF);
  wrDat(((uint16_t)y1) >> 8); wrDat((uint16_t)y1 & 0xFF);
  wrCmd(0x2C);   // RAM Write
}

// ---- 向已开窗口连写 count 个颜色 ----
void Lcd::fillWindow(uint16_t color, uint32_t count) {
  static uint8_t chunk[512];
  uint8_t hi = color >> 8, lo = color & 0xFF;
  for (int i = 0; i < (int)sizeof(chunk); i += 2) { chunk[i] = hi; chunk[i + 1] = lo; }

  digitalWrite(PIN_LCD_DC, HIGH);
  digitalWrite(PIN_LCD_CS, LOW);
  uint32_t bytes = count * 2;
  while (bytes > 0) {
    uint32_t n = bytes > sizeof(chunk) ? sizeof(chunk) : bytes;
    s_spi->writeBytes(chunk, n);
    bytes -= n;
  }
  digitalWrite(PIN_LCD_CS, HIGH);
}

void Lcd::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= _width || y >= _height) return;
  setWindow(x, y, x, y);
  fillWindow(color, 1);
}

void Lcd::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (w <= 0 || h <= 0) return;
  int16_t x1 = x + w - 1, y1 = y + h - 1;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x1 >= _width)  x1 = _width - 1;
  if (y1 >= _height) y1 = _height - 1;
  if (x1 < x || y1 < y) return;
  setWindow(x, y, x1, y1);
  fillWindow(color, (uint32_t)(x1 - x + 1) * (y1 - y + 1));
}

void Lcd::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  fillRect(x, y, w, 1, color);
}

void Lcd::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  fillRect(x, y, 1, h, color);
}
