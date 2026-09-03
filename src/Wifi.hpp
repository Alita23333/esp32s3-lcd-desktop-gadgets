#pragma once
#include <Arduino.h>

// 初始化 WiFi（modemSleep: true=射频休眠省电，适合定期同步；false=射频常开，适合持续收 UDP）
void Wifi_Init(bool modemSleep);

bool Wifi_IsConfigured();
bool Wifi_IsConnected();
String Wifi_GetIP();

// 每轮调用，推进连接/重连逻辑
void Wifi_Loop();
