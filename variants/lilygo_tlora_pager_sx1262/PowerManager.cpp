#include "PowerManager.h"

#include <WiFi.h>
#include <Wire.h>

// XL9555 bits (wiki pin list): IO3=NFC EN, IO5=NFC PWR, IO4=GNSS EN, IO14=SD EN
#define XL_BIT_NFC_EN  3
#define XL_BIT_NFC_PWR 5
#define XL_BIT_GNSS_EN 4

bool PowerManager::xl9555SetBit(uint8_t bit, bool on) {
  uint8_t regs[2];
  Wire.beginTransmission(0x20);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(0x20, (uint8_t)2) != 2) return false;
  regs[0] = Wire.read();
  regs[1] = Wire.read();
  uint8_t idx = bit / 8, b = 1 << (bit % 8);
  if (on)
    regs[idx] |= b;
  else
    regs[idx] &= ~b;
  Wire.beginTransmission(0x20);
  Wire.write(0x00);
  Wire.write(regs[0]);
  Wire.write(regs[1]);
  return Wire.endTransmission() == 0;
}

PowerManager &PowerManager::instance() {
  static PowerManager s;
  return s;
}

void PowerManager::begin() {
  // NFC (ST25R3916) is pure waste when idle — cut both enables.
  xl9555SetBit(XL_BIT_NFC_EN, false);
  xl9555SetBit(XL_BIT_NFC_PWR, false);

#if POWER_DISABLE_BLE
  WiFi.mode(WIFI_OFF);
  btStop();
#endif
  _lastActivity = millis();
}

// Backlight control. If TPagerST7796Display exposes setBrightness()/TFT_BL,
// prefer that; otherwise drive the AW9364 pin (42) with LEDC as here.
#define BL_PIN 42
void PowerManager::setBacklight(uint8_t level) {
  static bool ledc_ready = false;
  if (!ledc_ready) {
    ledcSetup(0, 5000, 8); // arduino-esp32 2.x LEDC API
    ledcAttachPin(BL_PIN, 0);
    ledc_ready = true;
  }
  ledcWrite(0, level);
  _backlight = level;
}

void PowerManager::onActivity() {
  _lastActivity = millis();
  if (_backlight == 0) setBacklight(_targetBrightness);
}

void PowerManager::setTargetBrightness(uint8_t level) {
  _targetBrightness = level;
  if (_backlight > 0) setBacklight(level); // apply immediately if screen is on
}

void PowerManager::screenOff() {
  setBacklight(0);
}

void PowerManager::loop() {
  uint32_t now = millis(), idle = (now - _lastActivity) / 1000;

  // Screen: dim -> off. Biggest single battery win.
  if (_backlight > 0) {
    if (idle >= SCREEN_OFF_SECS)
      setBacklight(0);
    else if (idle >= SCREEN_DIM_SECS)
      setBacklight(_targetBrightness / 4);
  }

  // GPS duty cycle: skipped entirely when GPS is disabled by the user.
  if (!_gpsDisabled) {
    if (now - _gpsWindowStart >= (_gpsOn ? GPS_ON_SECS : GPS_OFF_SECS) * 1000UL) {
      _gpsOn = !_gpsOn;
      _gpsWindowStart = now;
      xl9555SetBit(XL_BIT_GNSS_EN, _gpsOn);
    }
  }
}

void PowerManager::setGpsEnabled(bool en) {
  _gpsDisabled = !en;
  if (!en) {
    // Hard off immediately.
    _gpsOn = false;
    xl9555SetBit(XL_BIT_GNSS_EN, false);
  } else {
    // Resume duty cycle from now — turn on right away.
    _gpsOn = true;
    _gpsWindowStart = millis();
    xl9555SetBit(XL_BIT_GNSS_EN, true);
  }
}