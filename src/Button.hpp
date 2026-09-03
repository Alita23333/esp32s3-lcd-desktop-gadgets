#pragma once
#include <Arduino.h>

// 轻量去抖按键（低电平=按下，内部上拉）
// 用法: begin() 后每轮 loop 调用 pressed()，返回 true 表示确认了一次按下
struct Btn {
  uint8_t pin;
  bool lastRaw = HIGH;
  bool stable = HIGH;
  uint32_t lastChange = 0;
  static const uint16_t DEBOUNCE_MS = 30;

  explicit Btn(uint8_t p) : pin(p) {}

  void begin() {
    pinMode(pin, INPUT_PULLUP);
    lastRaw = stable = digitalRead(pin);
  }
  bool pressed() {
    bool raw = digitalRead(pin);
    if (raw != lastRaw) { lastRaw = raw; lastChange = millis(); }
    if (stable != lastRaw && (millis() - lastChange) >= DEBOUNCE_MS) {
      stable = lastRaw;
      if (stable == LOW) return true;   // 按下沿
    }
    return false;
  }
  bool isDown() const { return stable == LOW; }
};
