#ifndef PALA_STORAGE_CLICK_TIMINGS_H
#define PALA_STORAGE_CLICK_TIMINGS_H

#include <stdint.h>

#include "src/config.h"
#include "src/storage/kv_store.h"

namespace ClickTimings {

struct TimingSettingSpec {
  const char* key;
  uint32_t defaultValue;
  uint32_t (*current)();
  uint32_t (*minValue)();
  uint32_t (*maxValue)();
  void (*reset)();
  void (*set)(uint32_t value);
};

void loadSettings(KeyValueStore& kv);
void saveSettings(KeyValueStore& kv);
void loadSettings();
void saveSettings();

const TimingSettingSpec* timingSettings();
uint8_t timingSettingsCount();

uint32_t maxClickGapMs();
uint32_t maxClickSequenceMs();
uint32_t longMs();
uint32_t veryLongMs();
uint32_t debounceMs();

void setMaxClickGapMs(uint32_t value);
void setMaxClickSequenceMs(uint32_t value);
void setLongMs(uint32_t value);
void setVeryLongMs(uint32_t value);
void setDebounceMs(uint32_t value);

void resetMaxClickGapMs();
void resetMaxClickSequenceMs();
void resetLongMs();
void resetVeryLongMs();
void resetDebounceMs();
void resetToDefaults();

}  // namespace ClickTimings

#endif  // PALA_STORAGE_CLICK_TIMINGS_H
