#include "AudioManager.h"
#include <Wire.h>
#include <driver/i2s.h>   // legacy I2S API (espressif32 6.x / arduino-esp32 2.x)
#include <math.h>

static const i2s_port_t I2S_P = I2S_NUM_0;
static const uint8_t ES8311_ADDR = 0x18;

// ---- XL9555 (I2C 0x20) read-modify-write of the 16-bit output port ----
// Bits: IO3=NFC EN, IO4=GNSS EN, IO5=NFC PWR, IO14=SD EN (wiki pin list).
// IO1 is the presumed NS4150B speaker-amp gate (LilyGoLib BOARD_SPEAKER_EN).
// If your board rev has no amp gate, set AUDIO_PA_GATE=0.
#ifndef AUDIO_PA_GATE
#define AUDIO_PA_GATE 1
#endif
#define XL9555_BIT_PA 1

static bool xl9555_set_bit(uint8_t bit, bool on) {
    uint8_t regs[2];
    // Read current OUTPUT state (registers 0x02/0x03, not 0x00/0x01 which are INPUT/read-only)
    Wire.beginTransmission(0x20);
    Wire.write(0x02);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(0x20, (uint8_t)2) != 2) return false;
    regs[0] = Wire.read(); regs[1] = Wire.read();
    uint8_t idx = bit / 8, b = 1 << (bit % 8);
    if (on) regs[idx] |= b; else regs[idx] &= ~b;
    // Configure pin as output (direction regs 0x06/0x07, 0=output)
    uint8_t cfg[2];
    Wire.beginTransmission(0x20);
    Wire.write(0x06);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom(0x20, (uint8_t)2) == 2) {
        cfg[0] = Wire.read(); cfg[1] = Wire.read();
        cfg[idx] &= ~b; // set as output
        Wire.beginTransmission(0x20);
        Wire.write(0x06); Wire.write(cfg[0]); Wire.write(cfg[1]);
        Wire.endTransmission();
    }
    // Write output value
    Wire.beginTransmission(0x20);
    Wire.write(0x02); Wire.write(regs[0]); Wire.write(regs[1]);
    return Wire.endTransmission() == 0;
}

AudioManager& AudioManager::instance() {
    static AudioManager s;
    return s;
}

void AudioManager::codecWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg); Wire.write(val);
    Wire.endTransmission();
}

bool AudioManager::begin() {
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 48000,                 // matches the verified codec cfg
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4, .dma_buf_len = 256,
        .use_apll = true,
        .tx_desc_auto_clear = true,           // output zeros when TX queue empty; prevents DMA repeating last audio buffer
    };
    i2s_pin_config_t pins = {
        .mck_io_num = BOARD_I2S_MCLK,
        .bck_io_num = BOARD_I2S_BCK,
        .ws_io_num = BOARD_I2S_WS,
        .data_out_num = BOARD_I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE,
    };
    esp_err_t i2s_ret = i2s_driver_install(I2S_P, &cfg, 0, NULL);
    Serial.printf("[audio] i2s_driver_install: %s\n", esp_err_to_name(i2s_ret));
    if (i2s_ret != ESP_OK) return false;
    i2s_set_pin(I2S_P, &pins);

    // ES8311 min init — verbatim register set from M5Unified's
    // _speaker_enabled_cb_sticks3 (MCLK=BCLK mode). Works with the
    // ESP32 I2S master generating BCLK = 48000 x 32.
    codecWrite(0x00, 0x80);   // RESET + CSM power on
    codecWrite(0x01, 0xB5);   // CLOCK: MCLK source = BCLK
    codecWrite(0x02, 0x18);   // CLOCK: MULT_PRE = 3
    codecWrite(0x0D, 0x01);   // power up analog
    codecWrite(0x12, 0x00);   // power up DAC
    codecWrite(0x13, 0x20);   // HP drive enable (bit5=1); bit4=HP_MUTE must be 0
    setVolume(_volume);
    codecWrite(0x37, 0x08);   // DAC: bypass equalizer

    BaseType_t task_ret = xTaskCreate(audioTask, "audio", 4096, this, 1, &_audioTaskHandle);
    Serial.printf("[audio] xTaskCreate: %s  handle=%p\n", task_ret == pdPASS ? "OK" : "FAIL", _audioTaskHandle);
    return true;
}

void AudioManager::audioTask(void* arg) {
    AudioManager* self = (AudioManager*)arg;
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        int ev = self->_taskEvent;
        if (ev >= 0) self->chimeFor((SoundEvent)ev);
    }
}

void AudioManager::setVolume(uint8_t pct) {
    _volume = constrain(pct, 0, 100);
    // reg 0x32: 0xBF = 0 dB. Map 0..100 -> 0x00..-0 dB (linear dB scale).
    // Keep at 0 dB (0xBF); digital amplitude in tone() handles the level.
    codecWrite(0x32, 0xBF);
}

void AudioManager::paEnable(bool on) {
#if AUDIO_PA_GATE
    bool ok = xl9555_set_bit(XL9555_BIT_PA, on);
    Serial.printf("[audio] paEnable(%d) -> %s\n", on, ok ? "ok" : "FAIL");
#endif
}

void AudioManager::tone(int freq_hz, int ms) {
    static int16_t wav[256];
    const int sr = 48000;
    int16_t amp = (int16_t)(28000 * (_volume / 100.0f));
    for (int i = 0; i < 256; i++)
        wav[i] = (int16_t)(amp * sinf(2.0f * (float)M_PI * freq_hz * i / sr));

    paEnable(true);
    codecWrite(0x13, 0x20);                 // HP enable, unmute (bit5=1 bit4=0)
    vTaskDelay(pdMS_TO_TICKS(15));
    size_t w;
    // wav[256] is 256 int16_t values. For stereo I2S each write is 128 frames
    // (L+R pairs), so multiply by 2 so `ms` accurately maps to real duration.
    for (int s = 0; s < (sr * ms / 1000) * 2; s += 256)
        i2s_write(I2S_P, wav, sizeof(wav), &w, pdMS_TO_TICKS(100));
    // Flush a full DMA buffer of zeros. Static so it doesn't eat task stack.
    static int16_t zero[512];              // zero-initialised once at startup
    i2s_write(I2S_P, zero, sizeof(zero), &w, pdMS_TO_TICKS(100));
    codecWrite(0x13, 0x30);                 // HP enable, muted (bit5=1 bit4=1)
    paEnable(false);                        // amp off
}

void AudioManager::chimeFor(SoundEvent ev) {
    switch (ev) {
        case SoundEvent::MSG_DM:
            // Rising three-note arpeggio — A5 → C6 → E6
            tone(880,  150); vTaskDelay(pdMS_TO_TICKS(60));
            tone(1047, 150); vTaskDelay(pdMS_TO_TICKS(60));
            tone(1319, 300); break;
        case SoundEvent::MSG_CHANNEL:
            // Rising two-note — A5 → C6
            tone(880,  180); vTaskDelay(pdMS_TO_TICKS(70));
            tone(1047, 280); break;
        case SoundEvent::ADVERT:
        case SoundEvent::ACK:
            break; // silent
        case SoundEvent::LOW_BATTERY:
            tone(440, 200); vTaskDelay(pdMS_TO_TICKS(100)); tone(349, 250); break;
        case SoundEvent::BOOT:
            tone(523, 120); vTaskDelay(pdMS_TO_TICKS(60));  tone(784, 180); break;
    }
}

void AudioManager::notify(SoundEvent ev) {
    if (_enabled) _pending = (int)ev;
}

void AudioManager::loop() {
    int ev = _pending;
    if (ev < 0) return;
    _pending = -1;
    uint32_t now = millis();
    // _lastPlayMs == 0 means nothing has played yet — skip cooldown for first chime.
    if (_lastPlayMs != 0 && now - _lastPlayMs < COOLDOWN_MS) return;
    _lastPlayMs = now;
    _taskEvent = ev;
    Serial.printf("[audio] loop firing ev=%d handle=%p\n", ev, _audioTaskHandle);
    if (_audioTaskHandle) xTaskNotifyGive(_audioTaskHandle);
}