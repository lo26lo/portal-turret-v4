// Native tests of the pure logic (improvement D1): run on the computer, no board.
//   pio test -e native
// Needs a host C++ compiler (gcc / g++): present on Linux and in the CI;
// on Windows install MinGW-w64 first.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "logic/BoardLogic.h"
#include "logic/HallLogic.h"
#include "logic/MenuLogic.h"
#include "logic/RadarLogic.h"
#include "ui/MenuTree.h"

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

// ------------------------------------------------------------------ debug menu

static char lastCommand[64];
static int commandCount;
static int32_t startValue;

static void FakeExecute(void *, const char *command, char *answer, size_t size) {
  strncpy(lastCommand, command, sizeof(lastCommand) - 1);
  commandCount++;
  snprintf(answer, size, "ok %s\nsecond line", command);
}
static int32_t FakeValue(void *, const char *) { return startValue; }
static void FakeInfo(void *, int16_t page, uint8_t line, logic::Lang, char *text, size_t size) {
  snprintf(text, size, "p%d l%d", page, line);
}

#define T_ITEM(en, fr, kind, parent, command, save, key, page, min, max, step) \
  { en, fr, logic::ItemKind::kind, parent, command, save, key, page, min, max, step }
static const logic::MenuItem TEST_TREE[] = {
    T_ITEM("Root", "Racine", Menu, -1, nullptr, nullptr, nullptr, 0, 0, 0, 0),
    T_ITEM("Ping", "Ping", Command, 0, "ping", nullptr, nullptr, 0, 0, 0, 0),
    T_ITEM("Reset", "RAZ", Confirm, 0, "reset", nullptr, nullptr, 0, 0, 0, 0),
    T_ITEM("Info A", "Info A", Info, 0, nullptr, nullptr, nullptr, 0, 0, 0, 0),
    T_ITEM("Info B", "Info B", Info, 0, nullptr, nullptr, nullptr, 1, 0, 0, 0),
    T_ITEM("Vol", "Vol", Adjust, 0, "vol %d", "save %d", "v", 0, 0, 100, 10),
    T_ITEM("Sub", "Sous", Menu, 0, nullptr, nullptr, nullptr, 0, 0, 0, 0),
    T_ITEM("Deep", "Fond", Command, 6, "deep", nullptr, nullptr, 0, 0, 0, 0),
};

static logic::MenuNav MakeNav() {
  lastCommand[0] = '\0';
  commandCount = 0;
  startValue = 50;
  logic::MenuHooks hooks = {nullptr, FakeExecute, FakeValue, FakeInfo};
  return logic::MenuNav(TEST_TREE, 8, hooks);
}

static void Press(logic::MenuNav &nav, logic::MenuKey key, int times = 1) {
  for (int i = 0; i < times; i++) {
    nav.Key(key, 1000);
  }
}

void test_menu_render_root() {
  logic::MenuNav nav = MakeNav();
  logic::MenuScreen screen;
  nav.Render(screen, logic::Lang::French, 0);
  TEST_ASSERT_EQUAL_STRING("Racine", screen.line[0]);
  TEST_ASSERT_EQUAL_STRING("Ping", screen.line[1]);
  TEST_ASSERT_EQUAL_INT8(1, screen.highlight);
  TEST_ASSERT_EQUAL_STRING("A:suiv  B:ok", screen.line[5]);
  nav.Render(screen, logic::Lang::English, 0);
  TEST_ASSERT_EQUAL_STRING("Root", screen.line[0]);
  TEST_ASSERT_EQUAL_STRING("A:next  B:ok", screen.line[5]);
}

void test_menu_next_previous_wrap_and_scroll() {
  logic::MenuNav nav = MakeNav();
  logic::MenuScreen screen;
  Press(nav, logic::MenuKey::ALong); // from the first entry back to the last
  TEST_ASSERT_EQUAL_INT16(6, nav.GetSelected());
  nav.Render(screen, logic::Lang::English, 0);
  // Six entries, four lines: the window scrolled, the cursor is on the last line.
  TEST_ASSERT_EQUAL_STRING("Sub >", screen.line[4]);
  TEST_ASSERT_EQUAL_INT8(4, screen.highlight);
  Press(nav, logic::MenuKey::A); // wraps to the first
  TEST_ASSERT_EQUAL_INT16(1, nav.GetSelected());
}

void test_menu_command_and_answer() {
  logic::MenuNav nav = MakeNav();
  logic::MenuScreen screen;
  Press(nav, logic::MenuKey::B);
  TEST_ASSERT_EQUAL_STRING("ping", lastCommand);
  nav.Render(screen, logic::Lang::English, 1500);
  TEST_ASSERT_EQUAL_STRING("ok ping", screen.line[5]); // first line of the answer only
  nav.Render(screen, logic::Lang::English, 5000);
  TEST_ASSERT_EQUAL_STRING("A:next  B:ok", screen.line[5]); // the answer expires after 3 s
}

void test_menu_confirm() {
  logic::MenuNav nav = MakeNav();
  Press(nav, logic::MenuKey::A);
  Press(nav, logic::MenuKey::B);
  TEST_ASSERT_TRUE(nav.GetMode() == logic::MenuNav::Mode::Confirm);
  TEST_ASSERT_EQUAL_INT(0, commandCount);
  Press(nav, logic::MenuKey::A); // cancel
  TEST_ASSERT_TRUE(nav.GetMode() == logic::MenuNav::Mode::Browse);
  TEST_ASSERT_EQUAL_INT(0, commandCount);
  Press(nav, logic::MenuKey::B, 2); // ask again, confirm
  TEST_ASSERT_EQUAL_STRING("reset", lastCommand);
  TEST_ASSERT_EQUAL_INT(1, commandCount);
}

void test_menu_info_pages() {
  logic::MenuNav nav = MakeNav();
  logic::MenuScreen screen;
  Press(nav, logic::MenuKey::A, 2);
  Press(nav, logic::MenuKey::B);
  TEST_ASSERT_TRUE(nav.GetMode() == logic::MenuNav::Mode::Info);
  nav.Render(screen, logic::Lang::English, 0);
  TEST_ASSERT_EQUAL_STRING("Info A", screen.line[0]);
  TEST_ASSERT_EQUAL_STRING("p0 l0", screen.line[1]);
  TEST_ASSERT_EQUAL_STRING("p0 l3", screen.line[4]);
  Press(nav, logic::MenuKey::A); // next page
  nav.Render(screen, logic::Lang::English, 0);
  TEST_ASSERT_EQUAL_STRING("p1 l0", screen.line[1]);
  Press(nav, logic::MenuKey::A); // wraps to the first page, skipping the other kinds
  nav.Render(screen, logic::Lang::English, 0);
  TEST_ASSERT_EQUAL_STRING("p0 l0", screen.line[1]);
  Press(nav, logic::MenuKey::B);
  TEST_ASSERT_TRUE(nav.GetMode() == logic::MenuNav::Mode::Browse);
}

void test_menu_adjust() {
  logic::MenuNav nav = MakeNav();
  Press(nav, logic::MenuKey::A, 4);
  Press(nav, logic::MenuKey::B);
  TEST_ASSERT_TRUE(nav.GetMode() == logic::MenuNav::Mode::Adjust);
  TEST_ASSERT_EQUAL_INT32(50, nav.GetValue());
  Press(nav, logic::MenuKey::B);
  TEST_ASSERT_EQUAL_STRING("vol 60", lastCommand);
  Press(nav, logic::MenuKey::A, 2);
  TEST_ASSERT_EQUAL_STRING("vol 40", lastCommand);
  Press(nav, logic::MenuKey::B, 10); // clamped at the maximum
  TEST_ASSERT_EQUAL_INT32(100, nav.GetValue());
  Press(nav, logic::MenuKey::BLong); // save and leave
  TEST_ASSERT_EQUAL_STRING("save 100", lastCommand);
  TEST_ASSERT_TRUE(nav.GetMode() == logic::MenuNav::Mode::Browse);

  Press(nav, logic::MenuKey::B); // in again
  int before = commandCount;
  Press(nav, logic::MenuKey::ALong); // leave without saving
  TEST_ASSERT_EQUAL_INT(before, commandCount);
  TEST_ASSERT_TRUE(nav.GetMode() == logic::MenuNav::Mode::Browse);
}

void test_menu_submenu_and_back() {
  logic::MenuNav nav = MakeNav();
  logic::MenuScreen screen;
  Press(nav, logic::MenuKey::BLong); // at the root: nothing happens
  TEST_ASSERT_EQUAL_INT16(0, nav.GetMenu());
  Press(nav, logic::MenuKey::ALong);
  Press(nav, logic::MenuKey::B);
  TEST_ASSERT_EQUAL_INT16(6, nav.GetMenu());
  nav.Render(screen, logic::Lang::French, 0);
  TEST_ASSERT_EQUAL_STRING("Sous", screen.line[0]);
  TEST_ASSERT_EQUAL_STRING("Fond", screen.line[1]);
  Press(nav, logic::MenuKey::BLong);
  TEST_ASSERT_EQUAL_INT16(0, nav.GetMenu());
  TEST_ASSERT_EQUAL_INT16(6, nav.GetSelected()); // back on the entry we came from
}

void test_hall_percent() {
  // normal polarity: closed at 1500, open at 2500
  TEST_ASSERT_EQUAL_UINT8(0, logic::HallPercent(1500, 2500, 1500));
  TEST_ASSERT_EQUAL_UINT8(50, logic::HallPercent(2000, 2500, 1500));
  TEST_ASSERT_EQUAL_UINT8(100, logic::HallPercent(2500, 2500, 1500));
  TEST_ASSERT_EQUAL_UINT8(0, logic::HallPercent(900, 2500, 1500));    // clamped
  TEST_ASSERT_EQUAL_UINT8(100, logic::HallPercent(4000, 2500, 1500)); // clamped
  // reversed magnet: closed at 2800, open at 1200
  TEST_ASSERT_EQUAL_UINT8(0, logic::HallPercent(2800, 1200, 2800));
  TEST_ASSERT_EQUAL_UINT8(50, logic::HallPercent(2000, 1200, 2800));
  TEST_ASSERT_EQUAL_UINT8(100, logic::HallPercent(1200, 1200, 2800));
  // identical thresholds must not divide by zero
  TEST_ASSERT_EQUAL_UINT8(0, logic::HallPercent(2000, 2000, 2000));
}

void test_wing_animation_screen() {
  logic::MenuScreen screen;
  logic::RenderWings(screen, logic::Lang::French, 0, 0, true);
  TEST_ASSERT_TRUE(screen.wingAnimation);
  TEST_ASSERT_EQUAL_STRING("Ailes", screen.line[0]);
  TEST_ASSERT_EQUAL_STRING("       [](O)[]", screen.line[2]); // closed: panels against the body
  TEST_ASSERT_EQUAL_STRING("en mouvement...", screen.line[5]);

  logic::RenderWings(screen, logic::Lang::English, 100, 100, false);
  TEST_ASSERT_EQUAL_STRING("Wings", screen.line[0]);
  TEST_ASSERT_EQUAL_STRING("  []=====(O)=====[]", screen.line[2]); // fully open
  TEST_ASSERT_EQUAL_STRING("L 100%     R 100%", screen.line[4]);
  TEST_ASSERT_EQUAL_STRING("stopped", screen.line[5]);

  logic::RenderWings(screen, logic::Lang::English, 40, 250, true); // one side, value out of range
  TEST_ASSERT_EQUAL_UINT8(40, screen.leftPercent);
  TEST_ASSERT_EQUAL_UINT8(100, screen.rightPercent);
  TEST_ASSERT_EQUAL_STRING("     []==(O)=====[]", screen.line[2]);

  // A menu screen never carries the animation flag.
  logic::MenuNav nav = MakeNav();
  nav.Render(screen, logic::Lang::English, 0);
  TEST_ASSERT_FALSE(screen.wingAnimation);
}

// Number of characters of a UTF-8 string (accented letters count once).
static int Characters(const char *text) {
  int n = 0;
  for (; *text; text++) {
    n += ((uint8_t)*text & 0xC0) != 0x80 ? 1 : 0;
  }
  return n;
}

void test_real_menu_tree() {
  TEST_ASSERT_TRUE(ui::MENU_TREE[0].kind == logic::ItemKind::Menu);
  TEST_ASSERT_EQUAL_INT16(-1, ui::MENU_TREE[0].parent);
  for (int16_t i = 1; i < ui::MENU_TREE_COUNT; i++) {
    const logic::MenuItem &item = ui::MENU_TREE[i];
    TEST_ASSERT_TRUE_MESSAGE(item.parent >= 0 && item.parent < ui::MENU_TREE_COUNT, item.en);
    TEST_ASSERT_TRUE_MESSAGE(ui::MENU_TREE[item.parent].kind == logic::ItemKind::Menu, item.en);
    // 19 characters: a submenu adds " >" and the line has 21 columns.
    TEST_ASSERT_TRUE_MESSAGE(Characters(item.en) <= 19, item.en);
    TEST_ASSERT_TRUE_MESSAGE(Characters(item.fr) <= 19, item.fr);
    if (item.kind == logic::ItemKind::Adjust) {
      TEST_ASSERT_TRUE_MESSAGE(item.minValue < item.maxValue && item.step > 0, item.en);
      TEST_ASSERT_NOT_NULL_MESSAGE(strstr(item.command, "%d"), item.en);
    }
    if (item.kind == logic::ItemKind::Command || item.kind == logic::ItemKind::Confirm) {
      TEST_ASSERT_NOT_NULL_MESSAGE(item.command, item.en);
    }
  }
  // The menus are where MenuIndex says.
  TEST_ASSERT_EQUAL_STRING("Tests", ui::MENU_TREE[ui::MenuTests].en);
  TEST_ASSERT_EQUAL_STRING("Sound", ui::MENU_TREE[ui::MenuSound].en);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_menu_render_root);
  RUN_TEST(test_menu_next_previous_wrap_and_scroll);
  RUN_TEST(test_menu_command_and_answer);
  RUN_TEST(test_menu_confirm);
  RUN_TEST(test_menu_info_pages);
  RUN_TEST(test_menu_adjust);
  RUN_TEST(test_menu_submenu_and_back);
  RUN_TEST(test_real_menu_tree);
  RUN_TEST(test_hall_percent);
  RUN_TEST(test_wing_animation_screen);
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
