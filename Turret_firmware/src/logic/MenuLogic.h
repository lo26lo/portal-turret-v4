#pragma once

// Pure logic of the debug menu (improvement M2): two-button navigation and
// rendering as six lines of text, without Arduino. Used by ui/DebugUi and by
// the native tests (test/test_logic). The OLED (128x64, 6x10 font) shows
// 21 characters by 6 lines; the web page shows the same lines.
//
// Keys:           browsing a menu     information page    adjusting a value   confirmation   wizard
//   A  short      next entry          next page           value - step        cancel         skip the step
//   A  long       previous entry      previous page       leave, no save      cancel         previous step
//   B  short      enter / run         back                value + step        confirm        do the step
//   B  long       back to parent      back                save and leave      cancel         leave

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace logic {

enum class Lang : uint8_t { English, French };
enum class MenuKey : uint8_t { A, ALong, B, BLong };

enum class ItemKind : uint8_t {
  Menu,    // has children (entries whose parent is this one)
  Command, // runs `command`
  Confirm, // asks "sure?" then runs `command`
  Info,    // live page, lines given by the infoLine hook (and a drawing by infoGraphic)
  Adjust,  // integer value; `command` (printf format, one %d) runs at each change
  Wizard,  // guided sequence: each child is a step (its label is the instruction, up to three lines)
};

struct MenuItem {
  const char *en;
  const char *fr;
  ItemKind kind;
  int16_t parent;       // index of the parent menu (or wizard), -1 for the root
  const char *command;  // Command / Confirm / wizard step: command line. Adjust: format applied at each change
  const char *save;     // Adjust: format run when leaving with B long (nullptr = nothing to save)
  const char *valueKey; // Adjust: key given to the getValue hook for the starting value
  int16_t infoPage;     // Info: page number given to the hooks
  int32_t minValue;
  int32_t maxValue;
  int32_t step;
};

const uint8_t SCREEN_LINES = 6;
const uint8_t SCREEN_COLUMNS = 21;
const size_t SCREEN_LINE_BYTES = 48; // 21 characters, accented letters take two bytes
const uint8_t INFO_LINES = 4;
const size_t SCREEN_DATA_BYTES = 216; // QR code up to version 6 (41 x 41 modules), or 128 graph samples

// What the OLED draws instead of the content lines. The lines always hold a
// text version too, for the web page.
enum class Graphic : uint8_t {
  None,
  Wings, // turret with its wings at leftPercent / rightPercent
  Radar, // value[0..5] = x, y (mm) of three targets (y <= 0: none), value[6] = range mm, value[7] = half angle deg
  Graph, // data[0..dataLength-1] = samples 0..255, value[0] / value[1] = the two thresholds 0..255
  Qr,    // data = modules, row by row, MSB first; value[0] = modules per side
  Eye,   // value[0] = gaze -100..100, value[1] = mood (0 asleep, 1 awake, 2 angry)
};

struct MenuScreen {
  char line[SCREEN_LINES][SCREEN_LINE_BYTES]; // 0 = title, 1..4 = content, 5 = hint or answer
  int8_t highlight;                           // content line shown inverted, -1 = none
  Graphic graphic;
  uint8_t leftPercent;  // Wings: 0 closed .. 100 open
  uint8_t rightPercent;
  int16_t value[8];
  uint8_t dataLength;
  uint8_t data[SCREEN_DATA_BYTES];
};

// Number of characters of a UTF-8 string (accented letters count once).
inline int Utf8Length(const char *text) {
  int n = 0;
  for (; *text; text++) {
    n += ((uint8_t)*text & 0xC0) != 0x80 ? 1 : 0;
  }
  return n;
}

// Word wrap on `columns` characters (UTF-8 aware) into at most `maxLines`
// lines of SCREEN_LINE_BYTES. Returns the number of lines used.
inline uint8_t WrapText(const char *text, char lines[][SCREEN_LINE_BYTES], uint8_t maxLines, uint8_t columns) {
  uint8_t line = 0;
  while (*text == ' ') {
    text++;
  }
  while (*text && line < maxLines) {
    // Longest prefix of whole words that fits; a single long word is cut.
    const char *cut = nullptr;
    const char *p = text;
    int characters = 0;
    while (*p) {
      if (((uint8_t)*p & 0xC0) != 0x80) {
        if (characters == columns) {
          break;
        }
        characters++;
      }
      p++;
      if (*p == ' ' || *p == '\0') {
        cut = p;
      }
    }
    if (*p == '\0') {
      cut = p;
    } else if (cut == nullptr) {
      cut = p;
    }
    size_t bytes = (size_t)(cut - text);
    if (bytes > SCREEN_LINE_BYTES - 1) {
      bytes = SCREEN_LINE_BYTES - 1;
    }
    memcpy(lines[line], text, bytes);
    lines[line][bytes] = '\0';
    line++;
    text = cut;
    while (*text == ' ') {
      text++;
    }
  }
  return line;
}

// Screen shown while a wing moves: "[]===(O)===[]" with the gaps growing as
// the wings open, and the two percentages.
inline void RenderWings(MenuScreen &screen, Lang lang, uint8_t leftPercent, uint8_t rightPercent, bool moving) {
  memset(&screen, 0, sizeof(screen));
  screen.highlight = -1;
  screen.graphic = Graphic::Wings;
  screen.leftPercent = leftPercent > 100 ? 100 : leftPercent;
  screen.rightPercent = rightPercent > 100 ? 100 : rightPercent;
  bool french = lang == Lang::French;
  snprintf(screen.line[0], SCREEN_LINE_BYTES, "%s", french ? "Ailes" : "Wings");

  // The body "(O)" stays on columns 9..11; each wing slides up to 5 columns away.
  const int maxGap = 5;
  int left = screen.leftPercent * maxGap / 100;
  int right = screen.rightPercent * maxGap / 100;
  char *out = screen.line[2];
  int column = 0;
  for (; column < 9 - 2 - left; column++) {
    out[column] = ' ';
  }
  out[column++] = '[';
  out[column++] = ']';
  for (int i = 0; i < left; i++) {
    out[column++] = '=';
  }
  out[column++] = '(';
  out[column++] = 'O';
  out[column++] = ')';
  for (int i = 0; i < right; i++) {
    out[column++] = '=';
  }
  out[column++] = '[';
  out[column++] = ']';
  out[column] = '\0';

  snprintf(screen.line[4], SCREEN_LINE_BYTES, "%s %3u%%     %s %3u%%", french ? "G" : "L", screen.leftPercent,
           french ? "D" : "R", screen.rightPercent);
  snprintf(screen.line[5], SCREEN_LINE_BYTES, "%s",
           moving ? (french ? "en mouvement..." : "moving...") : (french ? "arrêtées" : "stopped"));
}

struct MenuHooks {
  void *context;
  // Runs a command line of the interpreter and writes a short answer.
  void (*execute)(void *context, const char *command, char *answer, size_t size);
  // Starting value of an Adjust entry.
  int32_t (*getValue)(void *context, const char *key);
  // One line (0..INFO_LINES-1) of an information page.
  void (*infoLine)(void *context, int16_t page, uint8_t line, Lang lang, char *text, size_t size);
  // Optional: lets an information page add a drawing (screen.graphic and its data).
  void (*infoGraphic)(void *context, int16_t page, MenuScreen &screen);
};

class MenuNav {
public:
  MenuNav(const MenuItem *items, int16_t count, MenuHooks hooks) : items(items), count(count), hooks(hooks) {}

  enum class Mode : uint8_t { Browse, Info, Adjust, Confirm, Wizard };

  Mode GetMode() const { return mode; }
  int16_t GetMenu() const { return menu; }
  int16_t GetSelected() const { return ChildAt(menu, cursor); }
  int32_t GetValue() const { return value; }
  int16_t GetWizardStep() const { return wizardStep; }

  void Key(MenuKey key, uint32_t now) {
    switch (mode) {
    case Mode::Browse:
      KeyBrowse(key, now);
      break;
    case Mode::Info:
      if (key == MenuKey::A || key == MenuKey::ALong) {
        StepInfo(key == MenuKey::A ? 1 : -1);
      } else {
        mode = Mode::Browse;
      }
      break;
    case Mode::Adjust:
      KeyAdjust(key, now);
      break;
    case Mode::Confirm:
      if (key == MenuKey::B) {
        Run(items[active].command, now);
      }
      mode = Mode::Browse;
      break;
    case Mode::Wizard:
      KeyWizard(key, now);
      break;
    }
  }

  void Render(MenuScreen &screen, Lang lang, uint32_t now) const {
    // Whole structure to zero: two identical screens must compare equal byte for byte.
    memset(&screen, 0, sizeof(screen));
    screen.highlight = -1;
    bool french = lang == Lang::French;
    bool showAnswer = answer[0] != '\0' && (int32_t)(answerUntil - now) > 0;

    if (mode == Mode::Browse) {
      Copy(screen.line[0], Label(menu, lang));
      int16_t n = ChildCount(menu);
      // Window of four entries that keeps the cursor visible.
      int16_t first = cursor < 4 ? 0 : cursor - 3;
      for (int16_t row = 0; row < 4 && first + row < n; row++) {
        int16_t item = ChildAt(menu, first + row);
        snprintf(screen.line[1 + row], SCREEN_LINE_BYTES, "%s%s", Label(item, lang),
                 items[item].kind == ItemKind::Menu ? " >" : "");
        if (first + row == cursor) {
          screen.highlight = 1 + row;
        }
      }
      Copy(screen.line[5], showAnswer ? answer : (french ? "A:suiv  B:ok" : "A:next  B:ok"));
      return;
    }

    if (mode == Mode::Wizard) {
      int16_t n = ChildCount(active);
      if (wizardStep >= n) {
        Copy(screen.line[0], Label(active, lang));
        Copy(screen.line[2], french ? "Terminé" : "Done");
        if (showAnswer) {
          Copy(screen.line[4], answer);
        }
        Copy(screen.line[5], french ? "B:retour" : "B:back");
        return;
      }
      snprintf(screen.line[0], SCREEN_LINE_BYTES, "%s %d/%d", Label(active, lang), wizardStep + 1, n);
      WrapText(Label(ChildAt(active, wizardStep), lang), &screen.line[1], 3, SCREEN_COLUMNS);
      if (showAnswer) {
        Copy(screen.line[4], answer);
      }
      Copy(screen.line[5], french ? "B:ok  A:passer" : "B:ok  A:skip");
      return;
    }

    Copy(screen.line[0], Label(active, lang));
    if (mode == Mode::Info) {
      for (uint8_t i = 0; i < INFO_LINES; i++) {
        if (hooks.infoLine != nullptr) {
          hooks.infoLine(hooks.context, items[active].infoPage, i, lang, screen.line[1 + i], SCREEN_LINE_BYTES);
        }
      }
      Copy(screen.line[5], french ? "A:page  B:retour" : "A:page  B:back");
      if (hooks.infoGraphic != nullptr) {
        hooks.infoGraphic(hooks.context, items[active].infoPage, screen);
      }
    } else if (mode == Mode::Adjust) {
      snprintf(screen.line[2], SCREEN_LINE_BYTES, "      <  %ld  >", (long)value);
      if (showAnswer) {
        Copy(screen.line[4], answer);
      }
      Copy(screen.line[5], items[active].save != nullptr
                               ? (french ? "A:- B:+ B long:sauve" : "A:- B:+ hold B:save")
                               : (french ? "A:- B:+ B long:fin" : "A:- B:+ hold B:done"));
    } else {
      Copy(screen.line[2], french ? "Confirmer ?" : "Are you sure?");
      Copy(screen.line[5], french ? "B:oui   A:non" : "B:yes   A:no");
    }
  }

private:
  const char *Label(int16_t item, Lang lang) const {
    return lang == Lang::French ? items[item].fr : items[item].en;
  }

  static void Copy(char *target, const char *text) {
    strncpy(target, text, SCREEN_LINE_BYTES - 1);
    target[SCREEN_LINE_BYTES - 1] = '\0';
  }

  int16_t ChildCount(int16_t parent) const {
    int16_t n = 0;
    for (int16_t i = 0; i < count; i++) {
      n += items[i].parent == parent ? 1 : 0;
    }
    return n;
  }

  // Index of the `ordinal`-th child of `parent`, -1 if there is none.
  int16_t ChildAt(int16_t parent, int16_t ordinal) const {
    for (int16_t i = 0; i < count; i++) {
      if (items[i].parent == parent && ordinal-- == 0) {
        return i;
      }
    }
    return -1;
  }

  int16_t OrdinalOf(int16_t item) const {
    int16_t ordinal = 0;
    for (int16_t i = 0; i < item; i++) {
      ordinal += items[i].parent == items[item].parent ? 1 : 0;
    }
    return ordinal;
  }

  void Run(const char *command, uint32_t now) {
    answer[0] = '\0';
    if (command != nullptr && hooks.execute != nullptr) {
      hooks.execute(hooks.context, command, answer, sizeof(answer));
    }
    // Keep the first line only: the screen has one line for it.
    char *newline = strchr(answer, '\n');
    if (newline != nullptr) {
      *newline = '\0';
    }
    answerUntil = now + 3000;
  }

  void KeyBrowse(MenuKey key, uint32_t now) {
    int16_t n = ChildCount(menu);
    if (key == MenuKey::BLong) {
      if (items[menu].parent >= 0) {
        cursor = OrdinalOf(menu);
        menu = items[menu].parent;
      }
      return;
    }
    if (n == 0) {
      return;
    }
    if (key == MenuKey::A) {
      cursor = (cursor + 1) % n;
      return;
    }
    if (key == MenuKey::ALong) {
      cursor = (cursor + n - 1) % n;
      return;
    }
    int16_t item = ChildAt(menu, cursor);
    switch (items[item].kind) {
    case ItemKind::Menu:
      menu = item;
      cursor = 0;
      break;
    case ItemKind::Command:
      Run(items[item].command, now);
      break;
    case ItemKind::Confirm:
      active = item;
      mode = Mode::Confirm;
      break;
    case ItemKind::Info:
      active = item;
      mode = Mode::Info;
      break;
    case ItemKind::Adjust:
      active = item;
      mode = Mode::Adjust;
      value = hooks.getValue != nullptr ? hooks.getValue(hooks.context, items[item].valueKey) : 0;
      value = Clamp(value, items[item]);
      answer[0] = '\0';
      break;
    case ItemKind::Wizard:
      active = item;
      mode = Mode::Wizard;
      wizardStep = 0;
      answer[0] = '\0';
      break;
    }
  }

  void KeyAdjust(MenuKey key, uint32_t now) {
    const MenuItem &item = items[active];
    if (key == MenuKey::A || key == MenuKey::B) {
      value = Clamp(value + (key == MenuKey::B ? item.step : -item.step), item);
      RunFormat(item.command, now);
      return;
    }
    if (key == MenuKey::BLong) {
      RunFormat(item.save, now);
    }
    mode = Mode::Browse;
  }

  void KeyWizard(MenuKey key, uint32_t now) {
    int16_t n = ChildCount(active);
    if (key == MenuKey::BLong || wizardStep >= n) {
      mode = Mode::Browse; // leave (on the "done" screen any key leaves)
      return;
    }
    if (key == MenuKey::ALong) {
      if (wizardStep > 0) {
        wizardStep--;
      }
      return;
    }
    if (key == MenuKey::B) {
      Run(items[ChildAt(active, wizardStep)].command, now);
    } else {
      answer[0] = '\0'; // step skipped
    }
    wizardStep++;
  }

  void RunFormat(const char *format, uint32_t now) {
    if (format == nullptr) {
      return;
    }
    char command[64];
    snprintf(command, sizeof(command), format, (int)value);
    Run(command, now);
  }

  static int32_t Clamp(int32_t v, const MenuItem &item) {
    return v < item.minValue ? item.minValue : (v > item.maxValue ? item.maxValue : v);
  }

  // Next / previous information page among the siblings.
  void StepInfo(int16_t direction) {
    int16_t parent = items[active].parent;
    int16_t n = ChildCount(parent);
    int16_t ordinal = OrdinalOf(active);
    for (int16_t tries = 0; tries < n; tries++) {
      ordinal = (ordinal + n + direction) % n;
      int16_t item = ChildAt(parent, ordinal);
      if (items[item].kind == ItemKind::Info) {
        active = item;
        cursor = ordinal;
        return;
      }
    }
  }

  const MenuItem *items;
  int16_t count;
  MenuHooks hooks;

  Mode mode = Mode::Browse;
  int16_t menu = 0;   // menu being browsed (the root is entry 0)
  int16_t cursor = 0; // ordinal of the selected child
  int16_t active = 0; // entry shown in Info / Adjust / Confirm / Wizard mode
  int16_t wizardStep = 0;
  int32_t value = 0;
  char answer[SCREEN_LINE_BYTES] = "";
  uint32_t answerUntil = 0;
};

} // namespace logic
