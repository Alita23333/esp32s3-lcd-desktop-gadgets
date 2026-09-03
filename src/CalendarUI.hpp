#pragma once
#include <Arduino.h>
#include "Lcd.hpp"
#include "Weather.hpp"

// 初始化（清屏、字体适配器）
void Cal_Begin(Lcd& lcd);

// 周期渲染（内部按需重绘：时间/日期每次，天气块仅数据变化时）
void Cal_Render(Lcd& lcd, const WeatherData& wx, bool wifiOk);
