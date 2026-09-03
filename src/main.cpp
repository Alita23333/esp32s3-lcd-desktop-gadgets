/*
 * ESP32-S3-LCD-1.47B — 2048 体感版（独立固件）
 *
 * 经典 2048 数字合并游戏。输入三通道：
 *   倾斜板子（陀螺仪分区 + 边沿触发，开始时自动标定中立姿态）
 *   4 颗方向按键：GP11 上 / GP10 下 / GP9 左 / GP8 右
 *   串口 115200：'w'/'a'/'s'/'d' 移动，'g' 新局，'p' 遥测(#B,分数,最高,结束,16格)
 *
 * 分数/最高分（NVS 掉电保存）；合并得分；到达 2048 提示；无路可走判定游戏结束
 */

#include <Arduino.h>
#include "Config.h"
#include "Lcd.hpp"
#include "Button.hpp"
#include "Game2048.hpp"

static Lcd s_lcd;
static Btn s_btnUp(PIN_BTN_START);
static Btn s_btnDown(PIN_BTN_RESET);
static Btn s_btnLeft(PIN_BTN_SKIP);
static Btn s_btnRight(PIN_BTN_RIGHT);

void setup() {
  // RGB LED（GPIO38）：先发一帧"全灭"覆盖灯珠锁存的旧颜色，再拉低数据线保持常灭
  neopixelWrite(38, 0, 0, 0);
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);

  Serial.begin(115200);
  delay(300);
  Serial.println("==== 2048 boot ====");

  s_lcd.begin();
  s_lcd.setBacklight(BACKLIGHT_FOCUS);   // 沿用 35% 桌面亮度
  Serial.println("[init] lcd ok");

  G2048_Begin(s_lcd);
  Serial.println("==== ready ====");
}

void loop() {
  if (s_btnUp.pressed())    G2048_Input('w');
  if (s_btnDown.pressed())  G2048_Input('s');
  if (s_btnLeft.pressed())  G2048_Input('a');
  if (s_btnRight.pressed()) G2048_Input('d');

  while (Serial.available()) {
    char c = (char)Serial.read();
    G2048_Input(c);
  }

  G2048_Loop(s_lcd);
  delay(2);
}
