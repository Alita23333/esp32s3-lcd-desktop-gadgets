#include "Ble.hpp"
#include "Config.h"
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLECharacteristic.h>
#include <NimBLEConnInfo.h>
#include <NimBLEAdvertisementData.h>

// ---- Nordic UART Service 标准 UUID ----
#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_CHAR_RX_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // 手机 -> 设备 (write)
#define NUS_CHAR_TX_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // 设备 -> 手机 (notify)

static void (*s_on_receive)(const uint8_t*, size_t) = nullptr;
static bool s_connected = false;
static NimBLECharacteristic* s_txChar = nullptr;

// ---- Server 回调 ----
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
    s_connected = true;
    Serial.printf("[BLE] 客户端已连接 (%s)\n", info.getAddress().toString().c_str());
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
    s_connected = false;
    Serial.printf("[BLE] 客户端断开 reason=%d，重新广播\n", reason);
    s->startAdvertising();
  }
};

// ---- 接收特征回调 ----
class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
    NimBLEAttValue v = c->getValue();
    if (v.length() == 0) return;
    if (s_on_receive) s_on_receive(v.data(), v.length());
  }
};

void Ble_Init() {
  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* service = server->createService(NUS_SERVICE_UUID);

  // RX（手机写）
  NimBLECharacteristic* rxChar = service->createCharacteristic(
      NUS_CHAR_RX_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rxChar->setCallbacks(new RxCallbacks());

  // TX（设备通知）；NimBLE 为 notify 自动处理 CCCD(2902)，无需手动添加
  s_txChar = service->createCharacteristic(
      NUS_CHAR_TX_UUID,
      NIMBLE_PROPERTY::NOTIFY);

  // NIMBLE 2.x: service 随 server start 自动启动

  // ---- 广播（参照官方 NimBLE_Server 示例标准做法）----
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(BLE_DEVICE_NAME);        // 设备名放入广播（手机扫描可见）
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->enableScanResponse(true);        // 支持扫描响应
  adv->start();

  Serial.printf("[BLE] 已启动，设备名=%s (NimBLE)\n", BLE_DEVICE_NAME);
}

bool Ble_IsConnected() { return s_connected; }

bool Ble_IsAdvertising() {
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  return adv ? adv->isAdvertising() : false;
}

void Ble_Loop(void (*on_receive)(const uint8_t*, size_t)) {
  s_on_receive = on_receive;
}

void Ble_Send(const uint8_t* data, size_t len) {
  if (!s_connected || !s_txChar) return;
  s_txChar->setValue(data, len);
  s_txChar->notify();
}

void Ble_Send(const char* cstr) {
  Ble_Send((const uint8_t*)cstr, strlen(cstr));
}
