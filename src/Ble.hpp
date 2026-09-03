#pragma once
#include <Arduino.h>

// 初始化 BLE，启动广播。手机可用 nRF Connect 等 App 连接
// 设备名来自 Config.h 的 BLE_DEVICE_NAME
void Ble_Init();

bool Ble_IsConnected();
bool Ble_IsAdvertising();

// 每轮调用：处理接收缓冲（调用用户回调）、维护连接状态
// on_receive: 收到手机发来的数据时回调 (data, len)
void Ble_Loop(void (*on_receive)(const uint8_t* data, size_t len));

// 设备主动向手机发数据（若已连接）
void Ble_Send(const uint8_t* data, size_t len);
void Ble_Send(const char* cstr);
