#pragma once
#include <Arduino.h>
#include "Lcd.hpp"

// 初始化（标题、启动首轮异步扫描）
void Radar_Begin(Lcd& lcd);

// 主循环：扫描状态机 + 雷达动画 + 列表渲染
void Radar_Loop(Lcd& lcd);

// 串口/按钮：'r' 强制重新扫描
void Radar_ForceScan();

// 串口：'p' 打印遥测（每网络一行 #W + 结束行 #WEND）
void Radar_PrintTelemetry();
