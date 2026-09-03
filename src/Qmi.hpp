#pragma once
#include <Arduino.h>

// 板载 QMI8658 六轴 IMU（I2C: SDA=48, SCL=47）最小驱动 —— 仅加速度计
bool Qmi_Begin();                                   // 初始化并自检，返回是否就绪
bool Qmi_GetAccel(float& x, float& y, float& z);    // 读取加速度，单位 g
