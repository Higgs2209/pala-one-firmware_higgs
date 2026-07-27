// Tests for src/storage/click_timings.cpp.

#include "test_framework.h"
#include "map_kv_store.h"
#include "storage/click_timings.h"

TEST_CASE("ClickTimings — defaults match firmware constants") {
  ClickTimings::resetToDefaults();

  CHECK_EQ(ClickTimings::maxClickGapMs(), MAX_CLICK_GAP_MS);
  CHECK_EQ(ClickTimings::maxClickSequenceMs(), MAX_CLICK_SEQUENCE_MS);
  CHECK_EQ(ClickTimings::longMs(), LONG_MS);
  CHECK_EQ(ClickTimings::veryLongMs(), VERY_LONG_MS);
  CHECK_EQ(ClickTimings::debounceMs(), DEBOUNCE_MS);
}

TEST_CASE("ClickTimings — round-trips through a KeyValueStore") {
  ClickTimings::resetToDefaults();
  ClickTimings::setMaxClickGapMs(321);
  ClickTimings::setMaxClickSequenceMs(654);
  ClickTimings::setLongMs(777);
  ClickTimings::setVeryLongMs(1777);
  ClickTimings::setDebounceMs(9);

  MapKvStore kv;
  ClickTimings::saveSettings(kv);

  ClickTimings::resetToDefaults();
  ClickTimings::loadSettings(kv);

  CHECK_EQ(ClickTimings::maxClickGapMs(), 321u);
  CHECK_EQ(ClickTimings::maxClickSequenceMs(), 654u);
  CHECK_EQ(ClickTimings::longMs(), 777u);
  CHECK_EQ(ClickTimings::veryLongMs(), 1777u);
  CHECK_EQ(ClickTimings::debounceMs(), 9u);
}

TEST_CASE("ClickTimings — long and very-long stay ordered") {
  ClickTimings::resetToDefaults();
  ClickTimings::setLongMs(1500);
  ClickTimings::setVeryLongMs(1000);

  CHECK_EQ(ClickTimings::longMs(), 1500u);
  CHECK_EQ(ClickTimings::veryLongMs(), 1501u);
}