#include "Qmi.hpp"
#include <Wire.h>

// 寄存器（QMI8658 数据手册）
#define QMI_ADDR   0x6B
#define QMI_REG_ID 0x00
#define QMI_CTRL1  0x02
#define QMI_CTRL2  0x03
#define QMI_CTRL6  0x05
#define QMI_CTRL7  0x08
#define QMI_AX_L   0x35

static float s_scale = 4.0f / 32768.0f;   // 量程 ±4g

static bool wr(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(QMI_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission(true) == 0;
}

static bool rd(uint8_t reg, uint8_t* buf, size_t n) {
  Wire.beginTransmission(QMI_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)QMI_ADDR, (int)n) != (int)n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

bool Qmi_Begin() {
  if (!Wire.begin(48, 47, 400000UL)) return false;
  delay(10);

  uint8_t id = 0;
  if (!rd(QMI_REG_ID, &id, 1)) return false;
  Serial.printf("[qmi] id=0x%02X\n", id);

  // 与 WaveShare 官方序列一致：CTRL1 开振荡器+地址自增；CTRL7 使能；CTRL6 关 AE
  uint8_t ctrl1 = 0;
  if (!rd(QMI_CTRL1, &ctrl1, 1)) return false;
  if (!wr(QMI_CTRL1, (ctrl1 & 0xFE) | 0x40)) return false;
  if (!wr(QMI_CTRL6, 0x00)) return false;
  if (!wr(QMI_CTRL7, 0x43)) return false;
  // CTRL2：量程 ±4G (bit4) | ODR 120Hz (0x06)
  if (!wr(QMI_CTRL2, 0x16)) return false;
  delay(20);   // 等首帧数据

  float x, y, z;
  if (!Qmi_GetAccel(x, y, z)) return false;
  Serial.printf("[qmi] accel %.2f %.2f %.2f g\n", x, y, z);
  return true;
}

bool Qmi_GetAccel(float& x, float& y, float& z) {
  uint8_t b[6];
  if (!rd(QMI_AX_L, b, 6)) return false;
  int16_t ax = (int16_t)((b[1] << 8) | b[0]);
  int16_t ay = (int16_t)((b[3] << 8) | b[2]);
  int16_t az = (int16_t)((b[5] << 8) | b[4]);
  x = ax * s_scale;
  y = ay * s_scale;
  z = az * s_scale;
  return true;
}
