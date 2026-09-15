// Shared low-voltage cutoff for BATTERY-powered boards (edge + bench tests).
//
// Why: a board left running on the cell and forgotten will keep draining it.
// Taking a Li-ion below ~2.5-3.0 V damages it (and is a recharge fire risk).
// This parks the ESP in deep sleep BEFORE that happens. Power-cycle to recover.
//
// This is a SOFT guard only — it is not a substitute for a protected cell / BMS.
// If the firmware crashes it can't protect anything; hardware protection still
// belongs on the cell. See MEMORY: battery-soft-cutoff.
//
// BATT_NO_PARK (build flag): disables the park entirely — volts() keeps working
// and still feeds the app, but isLow() always says false and parkForever() only
// warns. Set for demo builds so no firmware path can sleep a board mid-demo.
// With it set NOTHING in software protects the cells from over-discharge.
//
// REQUIRES a resistor divider from the battery to A0 so the pin never sees more
// than 3.3 V:   batt+ --[R_top]-- A0 --[R_bot]-- GND
//   single LiPo (4.2 V max): e.g. 100k / 220k     2S (~8.4 V): e.g. 100k / 47k
// Then CALIBRATE: set DIVIDER so volts() matches a multimeter on the cell.
//
// Safety against a board with NO divider wired (so A0 reads near 0): readings
// below SENSE_FLOOR are treated as "no battery sense" and the guard does
// nothing — it never false-trips a USB/bench board into sleep.
#pragma once
#include <Arduino.h>
#include <esp_sleep.h>

namespace batt {
  constexpr int   PIN         = A0;     // analog pin on the divider
  constexpr float DIVIDER     = 3.13f;  // Vbatt / Vpin — CALIBRATE to your divider
  constexpr float ADC_REF     = 3.3f;   // XIAO ADC full-scale
  constexpr float CUTOFF_V    = 3.20f;  // park the board below this (single cell)
  constexpr float SENSE_FLOOR = 2.50f;  // below this => assume no divider wired
  constexpr int   SAMPLES     = 8;      // averaged per reading

  // Averaged battery voltage (volts). ~2 ms per call.
  inline float volts() {
    analogReadResolution(12);                       // 0..4095
    uint32_t acc = 0;
    for (int i = 0; i < SAMPLES; i++) { acc += analogRead(PIN); delayMicroseconds(150); }
    float vpin = (acc / (float)SAMPLES / 4095.0f) * ADC_REF;
    return vpin * DIVIDER;
  }

  // True only if a plausible cell voltage is reading LOW (confirmed twice, so a
  // single noisy sample can't trip it). False if healthy OR if no sense wired.
  inline bool isLow() {
#ifdef BATT_NO_PARK
    return false;                                 // demo build: never park
#else
    float v = volts();
    if (v < SENSE_FLOOR || v >= CUTOFF_V) return false;
    delay(100);
    float v2 = volts();
    return (v2 >= SENSE_FLOOR && v2 < CUTOFF_V);
#endif
  }

  // Park in deep sleep forever. Power-cycle to recover. No wake timer on purpose:
  // a low cell should draw ~nothing, not keep waking to re-check.
  inline void parkForever() {
#ifdef BATT_NO_PARK
    Serial.printf("\n*** battery low (%.2fV) — park DISABLED (BATT_NO_PARK). "
                  "Staying awake; cells are UNPROTECTED. ***\n", volts());
    return;
#else
    Serial.printf("\n*** BATTERY LOW (%.2fV) — parking in deep sleep. "
                  "Power-cycle to recover. ***\n", volts());
    Serial.flush();
    esp_deep_sleep_start();
#endif
  }
}
