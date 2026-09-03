#pragma once
#include <Arduino.h>
#include "Lcd.hpp"

// 初始化（标题界面、NVS 最高分、陀螺仪）
void Snake_Begin(Lcd& lcd);

// 主循环：按键、陀螺仪转向、游戏节拍、渲染
void Snake_Loop(Lcd& lcd);

// 串口输入：'w'/'a'/'s'/'d' 转向，'g' 开始/重开，'p' 打印遥测行(#S,...)
void Snake_Input(char c);
void Snake_TogglePause();   // GP10 暂停/继续
