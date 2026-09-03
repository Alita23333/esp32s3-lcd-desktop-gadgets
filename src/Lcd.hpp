#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>

#define LCD_W 172
#define LCD_H 320

/*
 * ST7789 172x320 屏幕封装（Waveshare ESP32-S3-LCD-1.47B）
 * 基于 Adafruit_GFX：字体/图形可直接使用；底层窗口填充已优化。
 * 仅支持 rotation 0（竖屏 172 宽 × 320 高）。
 */
class Lcd : public Adafruit_GFX {
public:
  Lcd() : Adafruit_GFX(LCD_W, LCD_H) {}

  // 初始化引脚/SPI，并执行官方面板初始化序列
  void begin();

  // 设置背光占空比（0-100，用于昼夜自适应亮度）
  void setBacklight(uint8_t dutyPercent);

  // Adafruit_GFX 必需
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  // 优化：窗口批量填充（进度条/色块/清屏）
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;

private:
  void setWindow(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
  void fillWindow(uint16_t color, uint32_t count);
};
