#pragma once
#include <Arduino.h>
#include "Lcd.hpp"

// 初始化（棋盘框架、NVS 最高分、陀螺仪标定、新局）
void G2048_Begin(Lcd& lcd);

// 主循环：按键/倾斜输入、渲染
void G2048_Loop(Lcd& lcd);

// 串口输入：'w'/'a'/'s'/'d' 移动，'g' 新局，'p' 打印遥测(#B,分数,最高,结束,16格)
void G2048_Input(char c);
