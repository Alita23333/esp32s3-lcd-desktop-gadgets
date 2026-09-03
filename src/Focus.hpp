#pragma once
#include <Arduino.h>
#include "Lcd.hpp"

// 初始化（NVS 读取今日统计、清屏画静态框架）
void Focus_Begin(Lcd& lcd);

// 主循环：处理按键(GP9/GP8)、倒计时推进、变化驱动渲染
void Focus_Loop(Lcd& lcd);

// —— 串口调试/PC 控制接口（'s'/'r'/'t' 由 main 分发） ——
void Focus_ToggleStart();   // 开始/暂停
void Focus_ResetOrPreset(); // 待机切预设；运行/暂停重置
void Focus_QuickTest();     // 10 秒快速专注（验证流程用）
int  Focus_RemainingSec();  // 当前阶段剩余秒数（状态日志用）
