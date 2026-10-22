#pragma once
// Native in-race options screen (photo mode, the daylight tuner).
//
// Reuses the game's own in-race options package UI_PauseOptions.fng (title box, highlight bar,
// "Label  < Value >" rows) with our own screen object, the way NFSU2 Extra Options
// builds custom screens (LanguageSelectScreen.h: own factory in the FNGData table + own vtable) and
// NFSU2Unlimiter builds widget rows (BuyPerformanceParts.h: UIWidgetMenu + FEStatWidget, custom
// widget vtables, FEngPrintf on the row labels). Addresses/semantics: docs/kb/native-screen.md.
#include <cstddef>
#include <string>

namespace u2::optmenu {

struct Callbacks {
  // Any number: the package has 11 row slots (OPTION_NAME_1..11); with more rows the menu scrolls (a window of 11
  // onto the rows, with the native scroll bar at the bar's right edge).
  int rowCount;
  std::string (*label)(int row);
  std::string (*value)(int row);
  void (*adjust)(int row, int dir);              // LEFT/RIGHT on a row
  void (*activate)(int row);                     // ACCEPT / click on a row (called from frame code)
  void (*back)();                                // BACK pressed (called from frame code)
  bool (*isAction)(int row);                     // button row: no < > arrows, LEFT/RIGHT ignored
  // Optional: false = the row is shown (value still drawn live) but disabled the native way: the widget's Disable()
  // 0x51E3E0 plays the package's "Disable" script (grey text) and the menu cursor skips it (0x5390E0 loops past
  // widgets with +0x28 == 0). Checked every frame.
  bool (*isEnabled)(int row) = nullptr;
  // Optional: a short tip at the bottom of the bar, below the inner panel, in the package's own console "Back" prompt
  // text (BACK_BUTTON; its buffer grows, one line).
  const char* footer = nullptr;
};

// Sidebar layout, in FE canvas units (480 high, origin at the centre; with WidescreenFix the
// visible width is 480 * aspect). The package is drawn at native size; only the window frame is
// reshaped into a full-height bar at the left edge and the row columns are moved to fit it.
struct Layout {
  float width = 356;            // bar width (outer black backing); 340 until 2026-10-20 (room for the scroll bar)
  float margin = 16;            // gap between the bar and the screen edges
  float valueWidth = 110;       // width of the "< value >" column between the arrows
  int backingAlpha = 140;       // 0..255 alpha of the menu backing panels (transparency)
};

// How our screen gets built. UI_PauseOptions.fng has one factory slot in FNGData (0x7F81F4), and
// more than one ASI may use this screen:
// - Permanent: the slot points to our dispatching factory for the whole session (photo mode). Install
//   fails if the slot is not the game's own factory.
// - WhileOpening: the slot is borrowed only for the synchronous AddFngToUIObject_1 call inside Open()
//   and handed back right after; whatever factory sits there (the game's, or photo mode's) builds
//   every other construction. ASIs load in name order, so this mode never blocks a Permanent user.
enum class FactoryMode { Permanent, WhileOpening };

bool Install(const Callbacks& cb, const Layout& layout, bool verbose, FactoryMode mode = FactoryMode::Permanent);
void Uninstall();                                // puts the original factory back (Permanent)
bool IsMenuPackage(const char* name);  // our package while the menu is open
bool Open();                                     // call from frame code (not inside FE callbacks)
void Close();                                    // same
bool IsOpen();
// Hide/show without unloading: the package stays loaded (no rebuild hitch); while hidden it is
// not drawn (see IsHidden) and takes no input.
void SetHidden(bool hidden);
bool IsHidden();
// Shown but taking no pad / keyboard input (same port mask as SetHidden). A Back the screen receives starts the
// package's own fade-out of every row (verified-in-binary.md "PAD_BACK fades the whole UI_PauseOptions screen"), so
// block input while a key the owner handles itself (photo mode's Esc that cancels a capture) must not reach it.
void SetInputBlocked(bool blocked);
// The game's own keyboard map (the one in use before Open) while the menu stays loaded, e.g. to drive; false = the
// menu map again. Hide the menu meanwhile (SetHidden) so it takes no pad input.
void SetGameInput(bool on);
void Tick();                                     // every frame, even when photo mode is off
void Update(const std::string& title, const std::string& status);  // per frame: redraw, run deferred actions

// A row value's characters at most (OPTION_DATA_n BufferLength 32; FEPrintf cuts there, 0x537B56).
constexpr size_t kValueChars = 31;
// Long values end in "..." (user 2026-10-21): the whole text when fits(text), else the longest start of it (trailing
// spaces dropped) plus "..." that fits; tries are never longer than maxChars. fits is called for each try, longest
// first, and the last try (a lone "...") is taken whatever fits says. Returns the text taken.
template <typename Fits>
std::string FitText(const std::string& value, size_t maxChars, Fits fits) {
  for (size_t n = value.size();; --n) {
    std::string t = value.substr(0, n);
    if (n < value.size()) {
      while (!t.empty() && t.back() == ' ') t.pop_back();
      t += "...";
    }
    if (t.size() > maxChars && n > 0) continue;
    if (fits(t) || n == 0) return t;
  }
}

}  // namespace u2::optmenu
