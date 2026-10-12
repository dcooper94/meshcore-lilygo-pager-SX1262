#pragma once
#include <Arduino.h>

class PowerManager {
public:
    static PowerManager& instance();
    void begin();                 // kill NFC rail, init backlight policy
    void loop();                  // timeouts + GPS duty cycle
    void onActivity();            // call on ANY key/encoder event
    void screenOff();             // immediately kill backlight
    bool isScreenOff()    const { return _backlight == 0; }
    bool isGpsOn()        const { return _gpsOn; }
    bool isGpsEnabled()   const { return !_gpsDisabled; }
    void setGpsEnabled(bool en);

private:
    PowerManager() = default;
    static bool xl9555SetBit(uint8_t bit, bool on);   // RMW, shared chip-safe
    void setBacklight(uint8_t level);                 // 0..255 (or 16 steps)

    uint32_t _lastActivity = 0;
    uint8_t  _backlight = 255;
    bool     _gpsOn = false;
    bool     _gpsDisabled = true;
    uint32_t _gpsWindowStart = 0;
};