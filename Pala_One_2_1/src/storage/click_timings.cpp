#include "src/storage/click_timings.h"

#ifdef ARDUINO
#include "src/state.h"
#include "src/storage/preferences_store.h"
#endif

namespace ClickTimings {

static constexpr const char* kKeyGap = "cfg_cGap";
static constexpr const char* kKeySequence = "cfg_cSeq";
static constexpr const char* kKeyLong = "cfg_cLong";
static constexpr const char* kKeyVeryLong = "cfg_cVLng";
static constexpr const char* kKeyDebounce = "cfg_cDbnc";

static constexpr uint32_t kGapMin = 75;
static constexpr uint32_t kGapMax = 5000;
static constexpr uint32_t kSequenceMax = 10000;
static constexpr uint32_t kLongMin = 50;
static constexpr uint32_t kLongMax = 10000;
static constexpr uint32_t kVeryLongMax = 20000;
static constexpr uint32_t kDebounceMin = 0;
static constexpr uint32_t kDebounceMax = 100;

static uint32_t s_gapMs = MAX_CLICK_GAP_MS;
static uint32_t s_sequenceMs = MAX_CLICK_SEQUENCE_MS;
static uint32_t s_longMs = LONG_MS;
static uint32_t s_veryLongMs = VERY_LONG_MS;
static uint32_t s_debounceMs = DEBOUNCE_MS;

static uint32_t clampU32(uint32_t value, uint32_t minValue, uint32_t maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

static void normalize() {
  s_gapMs = clampU32(s_gapMs, kGapMin, kGapMax);
  s_sequenceMs = clampU32(s_sequenceMs, s_gapMs, kSequenceMax);
  s_longMs = clampU32(s_longMs, kLongMin, kLongMax);
  s_veryLongMs = clampU32(s_veryLongMs, s_longMs + 1, kVeryLongMax);
  s_debounceMs = clampU32(s_debounceMs, kDebounceMin, kDebounceMax);
}

static void loadFromStore(KeyValueStore& kv) {
  s_gapMs = (uint32_t)kv.getInt(kKeyGap, (int)MAX_CLICK_GAP_MS);
  s_sequenceMs = (uint32_t)kv.getInt(kKeySequence, (int)MAX_CLICK_SEQUENCE_MS);
  s_longMs = (uint32_t)kv.getInt(kKeyLong, (int)LONG_MS);
  s_veryLongMs = (uint32_t)kv.getInt(kKeyVeryLong, (int)VERY_LONG_MS);
  s_debounceMs = (uint32_t)kv.getInt(kKeyDebounce, (int)DEBOUNCE_MS);
  normalize();
}

static void saveToStore(KeyValueStore& kv) {
  normalize();
  kv.putInt(kKeyGap, (int)s_gapMs);
  kv.putInt(kKeySequence, (int)s_sequenceMs);
  kv.putInt(kKeyLong, (int)s_longMs);
  kv.putInt(kKeyVeryLong, (int)s_veryLongMs);
  kv.putInt(kKeyDebounce, (int)s_debounceMs);
}

void loadSettings(KeyValueStore& kv) {
  loadFromStore(kv);
}

void saveSettings(KeyValueStore& kv) {
  saveToStore(kv);
}

void loadSettings() {
#ifdef ARDUINO
  PreferencesStore kv(prefs);
  loadSettings(kv);
#endif
}

void saveSettings() {
#ifdef ARDUINO
  PreferencesStore kv(prefs);
  saveSettings(kv);
#endif
}

uint32_t maxClickGapMs() { return s_gapMs; }
uint32_t maxClickSequenceMs() { return s_sequenceMs; }
uint32_t longMs() { return s_longMs; }
uint32_t veryLongMs() { return s_veryLongMs; }
uint32_t debounceMs() { return s_debounceMs; }

void setMaxClickGapMs(uint32_t value) {
  s_gapMs = clampU32(value, kGapMin, kGapMax);
  if (s_sequenceMs < s_gapMs) s_sequenceMs = s_gapMs;
  saveSettings();
}

void setMaxClickSequenceMs(uint32_t value) {
  s_sequenceMs = clampU32(value, s_gapMs, kSequenceMax);
  saveSettings();
}

void setLongMs(uint32_t value) {
  s_longMs = clampU32(value, kLongMin, kLongMax);
  if (s_veryLongMs <= s_longMs) s_veryLongMs = s_longMs + 1;
  saveSettings();
}

void setVeryLongMs(uint32_t value) {
  s_veryLongMs = clampU32(value, s_longMs + 1, kVeryLongMax);
  saveSettings();
}

void setDebounceMs(uint32_t value) {
  s_debounceMs = clampU32(value, kDebounceMin, kDebounceMax);
  saveSettings();
}

void resetMaxClickGapMs() {
  s_gapMs = MAX_CLICK_GAP_MS;
  if (s_sequenceMs < s_gapMs) s_sequenceMs = s_gapMs;
  saveSettings();
}

void resetMaxClickSequenceMs() {
  s_sequenceMs = MAX_CLICK_SEQUENCE_MS;
  if (s_sequenceMs < s_gapMs) s_sequenceMs = s_gapMs;
  saveSettings();
}

void resetLongMs() {
  s_longMs = LONG_MS;
  if (s_veryLongMs <= s_longMs) s_veryLongMs = s_longMs + 1;
  saveSettings();
}

void resetVeryLongMs() {
  s_veryLongMs = VERY_LONG_MS;
  if (s_veryLongMs <= s_longMs) s_veryLongMs = s_longMs + 1;
  saveSettings();
}

void resetDebounceMs() {
  s_debounceMs = DEBOUNCE_MS;
  saveSettings();
}

void resetToDefaults() {
  s_gapMs = MAX_CLICK_GAP_MS;
  s_sequenceMs = MAX_CLICK_SEQUENCE_MS;
  s_longMs = LONG_MS;
  s_veryLongMs = VERY_LONG_MS;
  s_debounceMs = DEBOUNCE_MS;
  normalize();
  saveSettings();
}

}  // namespace ClickTimings