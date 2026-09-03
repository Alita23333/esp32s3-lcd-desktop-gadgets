#pragma once
#include <Arduino.h>
#include "Lcd.hpp"
#include "UdpMon.hpp"

// 画静态框架（头部/标签/分隔线），setup 时调用一次
void Dash_Begin(Lcd& lcd);

// 周期渲染（内部只重绘变化部分）。wifiOk 用于状态显示
void Dash_Render(Lcd& lcd, const GpuData& d, bool wifiOk);
