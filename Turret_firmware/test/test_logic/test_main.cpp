// Native tests of the pure logic (improvement D1): run on the computer, no board.
//   pio test -e native
// Needs a host C++ compiler (gcc / g++): present on Linux and in the CI;
// on Windows install MinGW-w64 first.

#include <stdint.h>
#include <unity.h>

#include "logic/BoardLogic.h"
#include "logic/HallLogic.h"
#include "logic/RadarLogic.h"

void setUp() {}
void tearDown() {}

// ------------------------------------------------------------------ radar

// Formula of the upstream parser (sensors/Radar.cpp before the refactoring).
static int16_t LegacyDecode(uint8_t low, uint8_t high) {
  int16_t value = (int16_t)(low | (high << 8));
  if (high & 0x80) {
    value -= 0x8000;
  } else {
    value = -value;
  }
  return value;
}

void test_decode_examples() {
  TEST_ASSERT_EQUAL_INT16(100, logic::DecodeLd2450(0x64, 0x80));
  TEST_ASSERT_EQUAL_INT16(-100, logic::DecodeLd2450(0x64, 0x00));
  TEST_ASSERT_EQUAL_INT16(0, logic::DecodeLd2450(0x00, 0x80));
  TEST_ASSERT_EQUAL_INT16(32767, logic::DecodeLd2450(0xFF, 0xFF));
  TEST_ASSERT_EQUAL_INT16(-32767, logic::DecodeLd2450(0xFF, 0x7F));
}

void test_decode_matches_upstream_parser() {
  for (uint32_t raw = 0; raw <= 0xFFFF; raw++) {
    uint8_t low = raw & 0xFF;
    uint8_t high = raw >> 8;
    if (logic::DecodeLd2450(low, high) != LegacyDecode(low, high)) {
      TEST_FAIL_MESSAGE("decode differs from the upstream parser");
    }
  }
}

void test_zone() {
  TEST_ASSERT_TRUE(logic::IsInZone(0, 1000, 3000, 45));      // straight ahead
  TEST_ASSERT_FALSE(logic::IsInZone(0, -1000, 3000, 45));    // behind
  TEST_ASSERT_FALSE(logic::IsInZone(0, 0, 3000, 45));        // on the radar
  TEST_ASSERT_FALSE(logic::IsInZone(0, 3100, 3000, 45));     // too far
  TEST_ASSERT_TRUE(logic::IsInZone(2000, 2100, 3000, 45));   // 43.6 deg, 2.9 m
  TEST_ASSERT_FALSE(logic::IsInZone(2100, 2000, 3000, 45));  // 46.4 deg
  TEST_ASSERT_FALSE(logic::IsInZone(-2100, 2000, 3000, 45)); // same on the other side
  TEST_ASSERT_TRUE(logic::IsInZone(-1000, 1000, 3000, 50));  // 45 deg inside 50
}

// ------------------------------------------------------------------ Hall

void test_hall_normal_polarity() {
  // open above 2500, closed below 1500
  TEST_ASSERT_TRUE(logic::HallPastOpen(2600, 2500, 1500));
  TEST_ASSERT_FALSE(logic::HallPastOpen(2000, 2500, 1500));
  TEST_ASSERT_TRUE(logic::HallPastClosed(1400, 2500, 1500));
  TEST_ASSERT_FALSE(logic::HallPastClosed(2000, 2500, 1500));
}

void test_hall_reversed_magnet() {
  // open below 1200, closed above 2800
  TEST_ASSERT_TRUE(logic::HallPastOpen(1100, 1200, 2800));
  TEST_ASSERT_FALSE(logic::HallPastOpen(2000, 1200, 2800));
  TEST_ASSERT_TRUE(logic::HallPastClosed(2900, 1200, 2800));
  TEST_ASSERT_FALSE(logic::HallPastClosed(2000, 1200, 2800));
}

void test_hall_rail() {
  TEST_ASSERT_TRUE(logic::HallAtRail(0));
  TEST_ASSERT_TRUE(logic::HallAtRail(4095));
  TEST_ASSERT_FALSE(logic::HallAtRail(2048));
}

// ------------------------------------------------------------------ buttons

void test_button_short_press() {
  logic::Debouncer button(30, 3000);
  button.Reset(false, 0);
  TEST_ASSERT_TRUE(button.Update(true, 10) == logic::PressEvent::None);
  TEST_ASSERT_TRUE(button.Update(true, 45) == logic::PressEvent::None); // now stable down
  TEST_ASSERT_TRUE(button.IsDown());
  TEST_ASSERT_TRUE(button.Update(false, 200) == logic::PressEvent::None);
  TEST_ASSERT_TRUE(button.Update(false, 235) == logic::PressEvent::ShortPress);
}

void test_button_bounce_ignored() {
  logic::Debouncer button(30, 3000);
  button.Reset(false, 0);
  button.Update(true, 10);
  button.Update(false, 20); // 10 ms spike
  TEST_ASSERT_TRUE(button.Update(false, 100) == logic::PressEvent::None);
  TEST_ASSERT_FALSE(button.IsDown());
}

void test_button_long_press_once() {
  logic::Debouncer button(30, 3000);
  button.Reset(false, 0);
  button.Update(true, 0);
  button.Update(true, 40);
  TEST_ASSERT_TRUE(button.Update(true, 3000) == logic::PressEvent::None);
  TEST_ASSERT_TRUE(button.Update(true, 3050) == logic::PressEvent::LongPress);
  TEST_ASSERT_TRUE(button.Update(true, 5000) == logic::PressEvent::None);
  button.Update(false, 5100);
  TEST_ASSERT_TRUE(button.Update(false, 5200) == logic::PressEvent::None); // no short press after a long one
}

void test_button_held_at_boot() {
  logic::Debouncer button(30, 3000);
  button.Reset(true, 0); // A + B at power-up: not an event
  TEST_ASSERT_TRUE(button.Update(true, 5000) == logic::PressEvent::None);
  button.Update(false, 5100);
  TEST_ASSERT_TRUE(button.Update(false, 5200) == logic::PressEvent::None);
}

// ------------------------------------------------------------------ board

void test_lowest_fault() {
  TEST_ASSERT_EQUAL_UINT8(0, logic::LowestFault(0));
  TEST_ASSERT_EQUAL_UINT8(1, logic::LowestFault(0x01));
  TEST_ASSERT_EQUAL_UINT8(3, logic::LowestFault(0x24)); // faults 3 and 6
  TEST_ASSERT_EQUAL_UINT8(6, logic::LowestFault(0x20));
}

void test_amp_gain_rounding() {
  TEST_ASSERT_EQUAL_INT32(9, logic::RoundAmpGain(0));
  TEST_ASSERT_EQUAL_INT32(9, logic::RoundAmpGain(10));
  TEST_ASSERT_EQUAL_INT32(12, logic::RoundAmpGain(11));
  TEST_ASSERT_EQUAL_INT32(12, logic::RoundAmpGain(13));
  TEST_ASSERT_EQUAL_INT32(15, logic::RoundAmpGain(14));
  TEST_ASSERT_EQUAL_INT32(15, logic::RoundAmpGain(40));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_decode_examples);
  RUN_TEST(test_decode_matches_upstream_parser);
  RUN_TEST(test_zone);
  RUN_TEST(test_hall_normal_polarity);
  RUN_TEST(test_hall_reversed_magnet);
  RUN_TEST(test_hall_rail);
  RUN_TEST(test_button_short_press);
  RUN_TEST(test_button_bounce_ignored);
  RUN_TEST(test_button_long_press_once);
  RUN_TEST(test_button_held_at_boot);
  RUN_TEST(test_lowest_fault);
  RUN_TEST(test_amp_gain_rounding);
  return UNITY_END();
}
