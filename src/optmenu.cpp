#include "u2sdk/optmenu.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "u2sdk/feng.h"
#include "u2sdk/fngchunk.h"
#include "u2sdk/jdlz.h"
#include "u2sdk/game.h"
#include "u2sdk/log.h"
#include "u2sdk/mem.h"

namespace u2::optmenu {
namespace {
using u2::Var;

// ---- game functions (see docs/kb/native-screen.md for the evidence behind each) -------------
constexpr const char* kPkg = "UI_PauseOptions.fng";
constexpr uintptr_t kFactorySlot = 0x7F81F4;      // FNGData[UI_PauseOptions.fng].create, orig 0x4EB650
constexpr uintptr_t kHelpMaskSlot = 0x7F81FC;     // FNGData[UI_PauseOptions.fng] help bar mask, 0x10400
constexpr uintptr_t kPauseKeyBlock = 0x861E78;    // Career+0x7138: JOY_EVENT_PAUSE handler ignores pause when set
constexpr uintptr_t kMenuKeyboardMap = 0x870754;  // byte written by 0x5B7ED0 (fe-input.md)
constexpr uintptr_t kJoyHandlersOn = 0x8384B8;    // set by AddJoyHandlers 0x52CDD0, cleared by 0x50C680
constexpr uintptr_t kKeyboardMapCtx = 0x870788;   // action map in use (1 = menu), cached by 0x5CFBF0
constexpr int kMaxRows = 11;
constexpr unsigned kMsgLeft = 0x9120409E, kMsgRight = 0xB5971BF1, kMsgAccept = 0x406415E3, kMsgBack = 0x911AB364,
                   kMsgSelect = 0xC407210, kMsgUp = 0x72619778, kMsgDown = 0x911C0A4B;

template <typename F>
F Fn(uintptr_t a) { return reinterpret_cast<F>(a); }
auto GameMalloc = Fn<void*(__cdecl*)(size_t)>(0x575620);
auto UIWidgetMenu_ctor = Fn<void*(__fastcall*)(void*, void*, void*)>(0x552780);       // thiscall(sd)
auto UIWidgetMenu_Notify = Fn<void(__fastcall*)(void*, void*, unsigned, void*, int, int)>(0x556020);
auto UIWidgetMenu_SetInitialOption = Fn<void(__fastcall*)(void*, void*, int)>(0x539650);  // Unlimiter InGameFunctions.h:182
auto UIWidgetMenu_AddToggleOption = Fn<int(__fastcall*)(void*, void*, void*, bool)>(0x54F170);
auto FEToggleWidget_ctor = Fn<void*(__fastcall*)(void*, void*, float*, float*, float*, float*, bool)>(0x51E230);
auto FEToggleWidget_Show = Fn<void(__fastcall*)(void*, void*)>(0x50D5D0);  // FEngSetVisible on title, data, arrows, backing
auto FEToggleWidget_BlinkArrows = Fn<void(__fastcall*)(void*, void*, unsigned)>(0x51E6B0);
auto FEngPrintf = Fn<void(__cdecl*)(void*, const char*, ...)>(0x537BE0);               // Unlimiter InGameFunctions.h:135
// An FEString's size is its current text measured in its own font times its Size scale, in FE canvas units, the units
// of the data column width (0x51D76F..0x51D7B2; verified-in-binary.md "FE text measuring"). Also the ScrollingThing
// ctor's call (0x542C2C, 0x542C7A).
auto FEngGetSize = Fn<void(__cdecl*)(void*, float*, float*)>(0x51D730);
auto AddFngToUIObject_1 = Fn<void(__cdecl*)(const char*, void*)>(0x555E80);            // push + AddJoyHandlers
auto RemoveFngAndJoyHandlers = Fn<void(__cdecl*)(const char*)>(0x558F00);              // what the game pairs with it
// Registers the FE joy handlers unless [0x8384B8] says they already are; thiscall(pUIData), as
// AddFngToUIObject_1 calls it at 0x555EA2..0x555EA8.
auto AddJoyHandlers = Fn<void(__fastcall*)(void*, void*)>(0x52CDD0);
auto IsFNGLoaded = Fn<bool(__cdecl*)(const char*)>(0x52CF60);
auto FEngSetLastButton = Fn<void(__cdecl*)(const char*, int)>(0x50B8C0);
// Selects the menu keyboard action map while racing (writes byte 0x870754, which the per-frame map
// selector 0x5CFBF0 checks). The game does the same for the in-world dyno screen (0x557AEE).
auto SetMenuKeyboardMap = Fn<void(__cdecl*)(char)>(0x5B7ED0);
using FactoryFn = void*(__cdecl*)(void*);
// FNG layouts (chunk 0x30203) are kept in the registry 0x8637B0. Lookup by name 0x51BB70 (thiscall, ret 4: the node or 0,
// and a found node moves to the head, 0x51BC14..0x51BC2F; FindFNGObject 0x51BD10 uses it so). Register 0x51BD30
// (cdecl(chunk): a node at the tail [0x8637B8], 0x51BCA0) and unregister 0x5425A0 (cdecl(chunk): the node whose chunk it
// is, 0x51BB30, 0x542530), the pair the bundle loader uses. The career mod registers its own layouts with 0x51BD30
// ([game] 2026-10-05, calendar_screen.cpp EnsureRegistered). Our FNGData entry (factory slot 0x7F81F4) already exists.
constexpr uintptr_t kLayoutRegistry = 0x8637B0;
auto FindLayout = Fn<void*(__fastcall*)(void*, void*, const char*)>(0x51BB70);
auto RegisterLayout = Fn<int(__cdecl*)(void*)>(0x51BD30);
auto UnregisterLayout = Fn<int(__cdecl*)(void*)>(0x5425A0);
// The game loads GLOBAL\InGameCommon.lzc (0x57DD04 -> loader 0x57CEF0; the .bun name is only copied as a label,
// 0x57DD25 -> 0x43DC00): JDLZ in a vanilla install (decoded byte-identical to the .bun), raw when a tool rewrote it
// (the `.net` copy, which has no .bun at all; [game] 2026-11-03). So the .lzc first, the .bun only as a fallback.
constexpr const char* kLayoutBundles[] = {"GLOBAL\\InGameCommon.lzc", "GLOBAL\\InGameCommon.bun"};

// ---- state ----------------------------------------------------------------------------------
Callbacks g_cb{};
Layout g_layout{};
FactoryFn g_origFactory = nullptr;  // Permanent: the game's factory. WhileOpening: the slot's value during Open
FactoryMode g_mode = FactoryMode::Permanent;
bool g_verbose = false;
bool g_wantOurs = false;     // next UI_PauseOptions.fng construction is ours
void* g_menu = nullptr;      // our UIWidgetMenu while open
bool g_open = false;
struct Row {
  uint8_t* widget;
  std::string label, value;
};
std::vector<Row> g_rows;  // one per widget, in slot order (at most kMaxRows)
// More rows than widgets (2026-10-20): the widgets are a window onto the rows, widget k shows row g_top + k (the
// career car list scrolls the same way, car_list.cpp, [game] 2026-10-06). The package has only 11 sets of row objects
// (AddToggleOption 0x54EF30 binds OPTION_NAME_<n> etc. by index; a 12th widget gets none), so the game's own scrolling,
// which hides whole widgets (0x52FDB0), cannot show more than 11 rows.
int g_rowCount = 0, g_top = 0;
bool Scrolls() { return g_rowCount > int(g_rows.size()); }
int g_pendingActivate = -1;
// The pause-key block byte is one game byte, but every ASI has its own copy of this module (photo mode, the daylight
// tuner). Its saved value, the pending restore and the count of open menus are therefore kept per process in a small
// named shared block, so one ASI's delayed restore never undoes another's open menu (review 2026-10-09).
struct SharedPause {
  volatile LONG openMenus;
  volatile LONG releaseIn;  // frames until the block byte is restored; every ASI's Tick counts it down
  uint8_t saved;
};
SharedPause* g_pause = nullptr;
SharedPause& Pause() {
  if (!g_pause) {
    char name[64];
    // v1: bump the number with any change to SharedPause (as freeze.cpp's block).
    std::snprintf(name, sizeof name, "Local\\NFSU2_u2sdk_optmenu_v1_%lu", GetCurrentProcessId());
    HANDLE h = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(SharedPause), name);
    if (h) g_pause = static_cast<SharedPause*>(MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedPause)));
    static SharedPause local{};  // no mapping: this ASI alone
    if (!g_pause) g_pause = &local;
  }
  return *g_pause;
}
int g_inputLogIn = 0;           // verbose: frames until the input state is logged
uint8_t g_savedKeyboardMap = 0;
bool g_joyHandlersWereOn = false;
bool g_pendingBack = false;
bool g_hidden = false;
bool g_inputBlocked = false;  // SetInputBlocked
bool g_gameInput = false;  // SetGameInput: the keyboard map saved at Open is back while the menu stays loaded
uint32_t g_savedPortMask;  // our package's pkg+0x24 while hidden or input-blocked
std::string g_title, g_status;
// Modal (Callbacks::modal): every other active package whose input we turned off, with its four masks as they were.
struct Masked {
  u2::feng::FEPackage* pkg;
  std::string name;
  uint32_t masks[4];
};
std::vector<Masked> g_masked;
// Our copy of UI_PauseOptions' layout outside races: the file's bytes, and the registered copy (the registry keeps a
// pointer to it, so it lives as long as it is registered; a fresh copy for each registration).
std::vector<uint8_t> g_layoutFile, g_layoutChunk;
bool g_layoutOurs = false;

void* g_menuVtbl[5];
void* g_toggleVtbl[17];

constexpr size_t kWidgetSize = 0x58;  // 0x54 FEToggleWidget + our row id (0x58 variants exist in-game)
int& RowId(void* w) { return *reinterpret_cast<int*>(static_cast<uint8_t*>(w) + 0x54); }
void* LabelString(void* w) { return *reinterpret_cast<void**>(static_cast<uint8_t*>(w) + 0x2C); }
void* ValueString(void* w) { return *reinterpret_cast<void**>(static_cast<uint8_t*>(w) + 0x30); }
bool IsAction(void* w) { return g_cb.isAction && g_cb.isAction(RowId(w)); }

Row* FindRow(void* w) {
  for (auto& r : g_rows)
    if (r.widget == w) return &r;
  return nullptr;
}

// A value wider than the data column ends in "..." (user 2026-10-21: long watermark names). The game draws a value
// centred on the column with no clipping, so a wider one runs over the arrows (0x52D800, 0x51C110), and it has no
// ellipsis of its own. Each try is printed and measured with the string's own font and scale (FEngGetSize); the
// OPTION_DATA_n buffers hold 31 characters (FEPrintf cuts there, 0x537B56), so no try is longer (FitText, unit-tested).
void PrintFitted(void* s, const std::string& value) {
  FitText(value, kValueChars, [s](const std::string& t) {
    FEngPrintf(s, "%s", t.c_str());
    float tw = 0, th = 0;  // stays 0 when the font is missing
    FEngGetSize(s, &tw, &th);
    return tw <= g_layout.valueWidth;
  });
}

// Draw: write label and value into the row's OPTION_NAME_n / OPTION_DATA_n strings.
void DrawRow(void* w, bool force) {
  int id = RowId(w);
  std::string label = g_cb.label(id), value = g_cb.value(id);
  Row* r = FindRow(w);
  if (r && !force && r->label == label && r->value == value) return;
  if (LabelString(w)) FEngPrintf(LabelString(w), "%s", label.c_str());
  if (ValueString(w)) PrintFitted(ValueString(w), value);
  if (r) {
    r->label = label;
    r->value = value;
  }
}

void __fastcall ToggleDraw(void* w, void*) { DrawRow(w, true); }

// ---- disabled rows (Callbacks::isEnabled) -------------------------------------------------------
// FE object scripts, as FEToggleWidget Disable 0x51E3E0 walks them: obj+0x4C first script, script+0x04 next,
// script+0x30 name hash (0x505450), obj+0x54 current script; a new current script is started with 0x517720
// (thiscall obj) and its time +0x10 reset to 0.
auto FEHashScriptName = Fn<unsigned(__cdecl*)(const char*)>(0x505450);
auto FEObjectStartScript = Fn<void(__fastcall*)(void*, void*)>(0x517720);
uint8_t* ScriptOf(void* obj, unsigned hash) {
  if (!obj) return nullptr;
  for (auto s = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(obj) + 0x4C); s; s = *reinterpret_cast<uint8_t**>(s + 4))
    if (*reinterpret_cast<unsigned*>(s + 0x30) == hash) return s;
  return nullptr;
}
uint8_t*& CurrentScript(void* obj) { return *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(obj) + 0x54); }
void PlayScript(void* obj, unsigned hash) {  // the body of 0x51E3E0's loop, for one object
  uint8_t* s = ScriptOf(obj, hash);
  if (!s) return;
  if (CurrentScript(obj) != s) {
    CurrentScript(obj) = s;
    FEObjectStartScript(obj, nullptr);
    *reinterpret_cast<uint32_t*>(CurrentScript(obj) + 0x10) = 0;
  }
  *reinterpret_cast<uint32_t*>(s + 0x10) = 0;
}

// The package has a "Disable" script only on rows 2, 3, 5 and 6 (OPTION_NAME_n / OPTION_DATA_n, FEngLib decode); with
// scrolling any row can sit in any slot ([game] 2026-10-20: Watermark Pos. did not grey). On the others the script's
// own grey is written straight into the object's colour (FEObject data +0x2C: B, G, R, A ints; dirty flag 0x400000,
// as FEngSetColor 0x51D843), every frame while disabled; "UnHighlight" (on every slot) puts white back on enable.
void SetGrey(void* obj, int v) {
  if (!obj) return;
  auto c = *reinterpret_cast<int**>(static_cast<uint8_t*>(obj) + 0x2C);
  if (!c || (c[0] == v && c[1] == v && c[2] == v)) return;
  c[0] = c[1] = c[2] = v;
  *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(obj) + 0x1C) |= 0x400000;
}

void SyncEnabled(uint8_t* w, bool want) {
  static const unsigned kDisable = FEHashScriptName("Disable"), kUnHighlight = FEHashScriptName("UnHighlight");
  void* name = *reinterpret_cast<void**>(w + 0x2C);
  void* data = *reinterpret_cast<void**>(w + 0x30);
  if (!want && name && !ScriptOf(name, kDisable)) {
    if (w[0x28]) {
      using SlotFn = void(__fastcall*)(void*, void*);
      reinterpret_cast<SlotFn>(g_toggleVtbl[8])(w, nullptr);  // +0x28 = 0 (the cursor skips it), arrows
    }
    SetGrey(name, 155);  // the Disable script's colours (OPTION_NAME_6 / OPTION_DATA_6)
    SetGrey(data, 150);
    return;
  }
  if (!want) {
    // FEToggleWidget Disable (slot 8). Again whenever the name's script is no longer "Disable": the package's
    // opening scripts run after the screen is built and replaced the grey of a row disabled at once ([game]
    // 2026-10-19: Focus was grey only after the first loop through the modes).
    bool greyed = name && ScriptOf(name, kDisable) && CurrentScript(name) == ScriptOf(name, kDisable);
    if (w[0x28] || (name && ScriptOf(name, kDisable) && !greyed)) {
      using SlotFn = void(__fastcall*)(void*, void*);
      reinterpret_cast<SlotFn>(g_toggleVtbl[8])(w, nullptr);
    }
    return;
  }
  if (w[0x28]) return;
  // Enable as slot 7 (0x51E270) does, but "UnHighlight" on the name and value only: on the arrows it fades them from
  // alpha 150 to 0, a flash on a row the cursor is not on ([game] 2026-10-19). Disable left them hidden.
  w[0x28] = 1;
  PlayScript(name, kUnHighlight);
  PlayScript(data, kUnHighlight);
  // A slot without a "Disable" script was greyed by hand (SetGrey) while its current script stayed "UnHighlight", and
  // restarting that script does not apply its colour again: the grey stayed until the cursor came ([game] 2026-10-20:
  // Watermark Pos. stayed grey, and Aperture looked disabled after scrolling into a slot Focus had greyed). White, the
  // UNHIGHLIGHT colour, by hand too. The cursor's row never comes through here (a disabled row is never the cursor's).
  if (name && !ScriptOf(name, kDisable)) {
    SetGrey(name, 255);
    SetGrey(data, 255);
  }
}

// Show (slot 5): button rows hide their arrow images (+0x4C/+0x50) again, as the widget's own
// Hide 0x50D610 does. The highlight scripts only animate colour, so the arrows stay hidden. Only button rows: the
// arrows of every other row are shown by alpha alone (LEFT/RIGHT_ARROW_n scripts: INIT and HIGHLIGHT 150, UNHIGHLIGHT
// fades to 0, so only the cursor's row shows them); hiding a disabled row's arrows here left them hidden for good
// when it was enabled again, as nothing makes them visible ([game] 2026-10-20: Focus lost its arrows).
void __fastcall ToggleShow(void* w, void*) {
  FEToggleWidget_Show(w, nullptr);
  if (!IsAction(w)) return;
  for (int off : {0x4C, 0x50})
    u2::feng::SetInvisible(*reinterpret_cast<u2::feng::FEObject**>(static_cast<uint8_t*>(w) + off));
}

// Act: same shape as the game's toggle rows (e.g. 0x4C3C70): change value, BlinkArrows, Draw.
void __fastcall ToggleAct(void* w, void*, const char* /*pkg*/, unsigned msg) {
  if (!static_cast<uint8_t*>(w)[0x28]) return;  // disabled (isEnabled): the menu already skips it; a click must too
  if ((msg == kMsgLeft || msg == kMsgRight) && !IsAction(w)) {
    g_cb.adjust(RowId(w), msg == kMsgRight ? 1 : -1);
    FEToggleWidget_BlinkArrows(w, nullptr, msg);
    DrawRow(w, true);
  } else if (msg == kMsgSelect) {
    g_pendingActivate = RowId(w);
  }
}

void HideScrollbar(const char* pkg);

// ---- scrolling (more rows than widgets) ---------------------------------------------------------
// The menu's own scroll bar: the ScrollingThing at menu+0x6C (ctor 0x542AC0 from the UIWidgetMenu ctor 0x5527C1).
// Update 0x538080 thiscall(first, last, current 1-based) ret 0xC; the base tick calls it for the widgets
// (0x544490), so after every base message we call it again for the rows. Arrow blink 0x495810 / 0x495850 thiscall()
// (verified-in-binary.md "Native scroll bar"; car_list.cpp).
auto ScrollBar_Update = Fn<void(__fastcall*)(void*, void*, int, int, int)>(0x538080);
auto ScrollBar_BlinkUp = Fn<void(__fastcall*)(void*, void*)>(0x495810);
auto ScrollBar_BlinkDown = Fn<void(__fastcall*)(void*, void*)>(0x495850);
uint8_t* Scroller(void* menu) { return static_cast<uint8_t*>(menu) + 0x6C; }
bool RowEnabled(int id) { return !g_cb.isEnabled || g_cb.isEnabled(id); }
int SlotOf(void* w) {
  for (size_t i = 0; i < g_rows.size(); ++i)
    if (g_rows[i].widget == w) return int(i);
  return -1;
}

// Point the widgets at rows top.. and redraw them at once (labels, values, arrows, grey).
void ShowRowsFrom(int top) {
  g_top = top;
  for (size_t i = 0; i < g_rows.size(); ++i) RowId(g_rows[i].widget) = top + int(i);
  for (auto& r : g_rows) {
    ToggleShow(r.widget, nullptr);
    DrawRow(r.widget, true);
    if (g_cb.isEnabled) SyncEnabled(r.widget, RowEnabled(RowId(r.widget)));
  }
}

// PAD_UP / PAD_DOWN when the rows scroll. Returns true when the message is handled here (not passed to the base).
// The next enabled row in that direction (none: stay, as the game's own list stops at its ends, 0x5391A7). Inside the
// window the base moves the cursor (it skips disabled widgets, +0x28). Outside, the rows shift so the target lands in
// the cursor's widget; when the list end stops the shift short, the base then moves the cursor the rest of the way
// over widgets that now show the disabled rows in between.
bool ScrollMove(void* self, unsigned msg) {
  void* cur = *reinterpret_cast<void**>(static_cast<uint8_t*>(self) + 0x54);
  int slot = SlotOf(cur);
  if (slot < 0) return false;
  const int dir = msg == kMsgDown ? 1 : -1, window = int(g_rows.size());
  int target = -1;
  for (int r = RowId(cur) + dir; r >= 0 && r < g_rowCount; r += dir)
    if (RowEnabled(r)) {
      target = r;
      break;
    }
  if (target < 0) return true;
  if (target >= g_top && target < g_top + window) return false;
  int top = (std::max)(0, (std::min)(target - slot, g_rowCount - window));
  ShowRowsFrom(top);
  if (dir > 0) ScrollBar_BlinkDown(Scroller(self), nullptr);
  else ScrollBar_BlinkUp(Scroller(self), nullptr);
  return target - top == slot;
}

void UpdateScrollbar(void* self) {
  const char* pkg = *reinterpret_cast<const char**>(static_cast<uint8_t*>(self) + 4);
  if (!Scrolls()) return HideScrollbar(pkg);
  void* cur = *reinterpret_cast<void**>(static_cast<uint8_t*>(self) + 0x54);
  ScrollBar_Update(Scroller(self), nullptr, 1, g_rowCount, cur ? RowId(cur) + 1 : 1);
}

// Screen message handler: base UIWidgetMenu first (navigation, LEFT/RIGHT -> row Act, tick).
void __fastcall MenuNotify(void* self, void*, unsigned msg, void* obj, int p1, int p2) {
  if (!(Scrolls() && (msg == kMsgUp || msg == kMsgDown) && ScrollMove(self, msg)))
    UIWidgetMenu_Notify(self, nullptr, msg, obj, p1, p2);
  UpdateScrollbar(self);
  if (msg == kMsgAccept) {
    void* cur = *reinterpret_cast<void**>(static_cast<uint8_t*>(self) + 0x54);
    if (cur) g_pendingActivate = RowId(cur);
  } else if (msg == kMsgBack) {
    g_pendingBack = true;  // never tear down from inside the FE update
  }
}

// ---- FE objects -------------------------------------------------------------------------------
// FEObject: +0x04 next sibling, +0x2C data, group +0x64 first child. Data (FEngLib BaseObject.Read):
// +0x00 colour, +0x10 pivot, +0x1C position, +0x28 rotation quaternion (x,y,z,w), +0x38 size.
// Positions are the object centre, relative to the parent group (topLeft = pos - size/2, 0x51D09E).
uint8_t* Next(uint8_t* o) { return *reinterpret_cast<uint8_t**>(o + 4); }
uint8_t* FirstChild(uint8_t* g) { return *reinterpret_cast<uint8_t**>(g + 0x64); }
float* Pos(uint8_t* o) { return reinterpret_cast<float*>(*reinterpret_cast<uint8_t**>(o + 0x2C) + 0x1C); }
float* Rot(uint8_t* o) { return reinterpret_cast<float*>(*reinterpret_cast<uint8_t**>(o + 0x2C) + 0x28); }
float* Size(uint8_t* o) { return reinterpret_cast<float*>(*reinterpret_cast<uint8_t**>(o + 0x2C) + 0x38); }
uint32_t& Flags(uint8_t* o) { return *reinterpret_cast<uint32_t*>(o + 0x1C); }
constexpr uint32_t kObjDirty = 0x400000;  // needs update (FEngSetColor 0x51D843, 0x4C6793)
constexpr uint32_t kObjGroup = 5;         // type at +0x18

uint8_t* Obj(const char* pkg, const char* name) {
  auto o = reinterpret_cast<uint8_t*>(u2::feng::FindObject(pkg, u2::feng::HashUpper(name)));
  if (!o) U2LOG("menu: %s not found", name);
  return o;
}

// Backing panels only have an INIT script, so alpha changes after startup stick.
void SetAlphaRecursive(uint8_t* obj, int alpha) {
  for (; obj; obj = Next(obj)) {
    if (auto data = *reinterpret_cast<int**>(obj + 0x2C)) {
      data[3] = data[3] * alpha / 255;  // FEObjData color: B, G, R, A ints
      Flags(obj) |= kObjDirty;
    }
    if (*reinterpret_cast<uint32_t*>(obj + 0x18) == kObjGroup) SetAlphaRecursive(FirstChild(obj), alpha);
  }
}

void ApplyBacking(const char* pkg) {
  if (g_layout.backingAlpha >= 255) return;
  for (const char* name : {"BIG MENU BACKING", "SMALLER MENU BACKING"})
    if (uint8_t* group = Obj(pkg, name)) SetAlphaRecursive(FirstChild(group), g_layout.backingAlpha);
}

// ---- sidebar layout ---------------------------------------------------------------------------
// Everything stays at native size: the window frame is reshaped into a full-height bar at the left
// edge and the row columns are moved into it. Objects are changed with the engine's own setters,
// as the game does at 0x4C6793 (flags |= 0x400000; SetSize; SetPosition). Both rewrite the base key
// of the matching track in every script (0x5178D0), so INIT and the other scripts keep the new values.
auto FEObject_SetPosition = Fn<void(__fastcall*)(void*, void*, const float*, bool)>(0x517C10);  // thiscall, ret 8
auto FEObject_SetSize = Fn<void(__fastcall*)(void*, void*, const float*, bool)>(0x517DB0);      // thiscall, ret 8
// Frame edges are 8 x 574 images turned a quarter about Z (quaternion z = +-0.707), so their
// on-screen width and height are swapped.
bool QuarterTurn(uint8_t* o) { return std::fabs(std::fabs(Rot(o)[2]) - 0.70710678f) < 0.05f; }

// w/h are magnitudes; the original sign is kept (negative sizes mirror an image).
void SetGeometry(uint8_t* o, float x, float y, float w, float h) {
  float pos[3] = {x, y, Pos(o)[2]};
  float size[3] = {std::copysign(w, Size(o)[0]), std::copysign(h, Size(o)[1]), Size(o)[2]};
  Flags(o) |= kObjDirty;
  FEObject_SetSize(o, nullptr, size, false);
  FEObject_SetPosition(o, nullptr, pos, false);
}

struct Rect {
  float l, t, r, b;
};

// Screen-space extent of a group's children (origin = the group's absolute position).
bool ChildBounds(uint8_t* group, float ox, float oy, Rect& out) {
  bool any = false;
  for (uint8_t* c = FirstChild(group); c; c = Next(c)) {
    float hw = std::fabs(Size(c)[0]) / 2, hh = std::fabs(Size(c)[1]) / 2;
    if (QuarterTurn(c)) std::swap(hw, hh);
    float x = ox + Pos(c)[0], y = oy + Pos(c)[1];
    Rect r{x - hw, y - hh, x + hw, y + hh};
    out = any ? Rect{(std::min)(out.l, r.l), (std::min)(out.t, r.t), (std::max)(out.r, r.r), (std::max)(out.b, r.b)} : r;
    any = true;
  }
  return any;
}

// 9-slice mapping along one axis: pieces spanning the middle stretch (keeping their distance to
// both edges); the others keep their size and follow the nearer edge.
void MapAxis(float lo, float hi, float oldLo, float oldHi, float newLo, float newHi, float& outC, float& outE) {
  if (hi - lo > (oldHi - oldLo) / 2) {
    lo = newLo + (lo - oldLo);
    hi = newHi - (oldHi - hi);
  } else {
    float d = (lo + hi) / 2 < (oldLo + oldHi) / 2 ? newLo - oldLo : newHi - oldHi;
    lo += d;
    hi += d;
  }
  outC = (lo + hi) / 2;
  outE = (hi - lo) / 2;
}

// Reshape one frame group (rounded box / outline). Its distance to the reference box is kept.
void ReshapeFrame(uint8_t* group, float ox, float oy, const Rect& ref, const Rect& bar) {
  Rect old;
  if (!group || !ChildBounds(group, ox, oy, old)) return;
  Rect dst{bar.l + (old.l - ref.l), bar.t + (old.t - ref.t), bar.r - (ref.r - old.r), bar.b - (ref.b - old.b)};
  for (uint8_t* c = FirstChild(group); c; c = Next(c)) {
    float hw = std::fabs(Size(c)[0]) / 2, hh = std::fabs(Size(c)[1]) / 2;
    bool turned = QuarterTurn(c);
    if (turned) std::swap(hw, hh);
    float x = ox + Pos(c)[0], y = oy + Pos(c)[1], nx, ny, nhw, nhh;
    MapAxis(x - hw, x + hw, old.l, old.r, dst.l, dst.r, nx, nhw);
    MapAxis(y - hh, y + hh, old.t, old.b, dst.t, dst.b, ny, nhh);
    if (turned) std::swap(nhw, nhh);
    SetGeometry(c, nx - ox, ny - oy, 2 * nhw, 2 * nhh);
  }
}

void HideObject(const char* pkg, const char* name) {
  if (auto o = u2::feng::FindObject(pkg, u2::feng::HashUpper(name))) u2::feng::SetInvisible(o);
}

// ---- footer tip (Callbacks::footer) ------------------------------------------------------------
// The package's console back prompt: group UI_COMMON_BACK (in NON_INTERACTIVE_GROUP) holding the button glyph
// JOY_EVENT_CANCEL and the text BACK_BUTTON (white, scale 0.7, 22-byte buffer; FEngLib decode). They are ConsoleOnly,
// which on PC only means "made invisible at load" (0x51C860 -> FEngSetInvisible 0x50CA00), so the text can be shown
// again with FEngSetVisible 0x50CA50. Its formatting (FEString +0x6C, tag 'Sj', 0x53501F) has word wrap (4) with no
// max width; cleared, so the tip stays on one line, left-aligned like the row labels (format 0).
auto FEngSetVisible = Fn<void(__cdecl*)(void*)>(0x50CA50);
constexpr uint32_t kObjInvisible = 1;
// BACK_BUTTON holds 22 characters, and FEPrintf cuts at that (0x537B30). FEString::SetString 0x535FE0 (thiscall,
// ret 4) grows the buffer itself; then the custom-text flag 2, as 0x537B70 does (verified-in-binary.md "FEPrintf cuts
// the text at the object's buffer"; the career mod's shop_class.cpp SetText, same steps).
auto FEStringSetString = Fn<void(__fastcall*)(void*, void*, const char*)>(0x535FE0);

void ShowFooter(const char* pkg) {
  auto group = reinterpret_cast<uint8_t*>(u2::feng::FindObject(pkg, u2::feng::HashUpper("UI_COMMON_BACK")));
  auto text = reinterpret_cast<uint8_t*>(u2::feng::FindObject(pkg, u2::feng::HashUpper("BACK_BUTTON")));
  auto glyph = reinterpret_cast<uint8_t*>(u2::feng::FindObject(pkg, u2::feng::HashUpper("JOY_EVENT_CANCEL")));
  for (uint8_t* o : {group, text})
    if (o && (Flags(o) & kObjInvisible)) FEngSetVisible(o);
  if (glyph && !(Flags(glyph) & kObjInvisible)) u2::feng::SetInvisible(reinterpret_cast<u2::feng::FEObject*>(glyph));
}

void SetFooterText(uint8_t* text) {
  if (*reinterpret_cast<int*>(text + 0x18) == 2) {  // a text object (0x537BBE)
    FEStringSetString(text, nullptr, g_cb.footer);
    *reinterpret_cast<uint32_t*>(text + 0x1C) |= 2;
  }
}

void PlaceFooter(const char* pkg, float x, float y) {
  uint8_t* nig = Obj(pkg, "NON_INTERACTIVE_GROUP ");
  uint8_t* group = Obj(pkg, "UI_COMMON_BACK");
  uint8_t* text = Obj(pkg, "BACK_BUTTON");
  if (!nig || !group || !text) return;
  *reinterpret_cast<uint32_t*>(text + 0x6C) &= ~4u;  // no word wrap
  float gx = Pos(nig)[0] + Pos(group)[0], gy = Pos(nig)[1] + Pos(group)[1];
  SetGeometry(text, x - gx, y - gy, Size(text)[0], Size(text)[1]);
  SetFooterText(text);
  ShowFooter(pkg);
}

// The scroll bar is shown again by the base handler every tick (0x544490 -> 0x538080), so this
// runs after every base message while all rows fit (UpdateScrollbar).
void HideScrollbar(const char* pkg) {
  for (const char* n : {"SCROLLBAR_BACKING", "SCROLLBAR_HANDLE", "SCROLLBAR_ARROW_1", "SCROLLBAR_ARROW_2"})
    HideObject(pkg, n);
}

// Called after the UIWidgetMenu ctor (which read the row geometry from TITLE_MASTER/DATA_MASTER,
// 0x51FF80) and before the rows are added (each row copies these fields, FEStatWidget ctor 0x51E1B0).
auto FEngGetTopLeft = Fn<void(__cdecl*)(void*, float*, float*)>(0x51D070);  // as the ScrollingThing ctor (0x542C1B)

// The scroll bar along the bar's right edge, beside the rows (native pieces and spacing: arrows 23 beyond the backing's
// ends, the corner caps 4 beyond, as in the package and UI_PC_Customize_Options). The ScrollingThing read the
// backing's top-left and size and the handle's size once in its ctor, before this move, so they are read again the
// same way.
void PlaceScrollbar(uint8_t* m, const char* pkg, float x, float top, float bottom) {
  uint8_t* back = Obj(pkg, "SCROLLBAR_BACKING");
  uint8_t* handle = Obj(pkg, "SCROLLBAR_HANDLE");
  uint8_t* up = Obj(pkg, "SCROLLBAR_ARROW_1");
  uint8_t* down = Obj(pkg, "SCROLLBAR_ARROW_2");
  uint8_t* nig = Obj(pkg, "NON_INTERACTIVE_GROUP ");
  if (!back || !handle || !up || !down || !nig) return;
  // Positions are centres; the arrows are 24 x 24 (INIT Size, refs/data/UI_PauseOptions.json), so +-12 puts their
  // outer edges on the rows' top and bottom.
  const float upY = top + 12, downY = bottom - 12, backT = upY + 23, backB = downY - 23;
  SetGeometry(up, x, upY, Size(up)[0], Size(up)[1]);
  SetGeometry(down, x, downY, Size(down)[0], Size(down)[1]);
  SetGeometry(back, x, (backT + backB) / 2, Size(back)[0], backB - backT);
  SetGeometry(handle, x, backT, Size(handle)[0], Size(handle)[1]);
  const float nx = Pos(nig)[0], ny = Pos(nig)[1];
  if (uint8_t* c = Obj(pkg, "U2_SCROLLER_CORNER")) SetGeometry(c, x - nx, backT - 4 - ny, Size(c)[0], Size(c)[1]);
  if (uint8_t* c = Obj(pkg, "U2_SCROLLER_CORNER(65)")) SetGeometry(c, x - nx, backB + 4 - ny, Size(c)[0], Size(c)[1]);
  uint8_t* s = Scroller(m);
  void* sBack = *reinterpret_cast<void**>(s + 0x40);
  void* sHandle = *reinterpret_cast<void**>(s + 0x44);
  if (sBack) {
    FEngGetTopLeft(sBack, reinterpret_cast<float*>(s + 0x1C), reinterpret_cast<float*>(s + 0x20));
    FEngGetSize(sBack, reinterpret_cast<float*>(s + 0x24), reinterpret_cast<float*>(s + 0x28));
  }
  if (sHandle) FEngGetSize(sHandle, reinterpret_cast<float*>(s + 0x2C), reinterpret_cast<float*>(s + 0x30));
}

// rows: widgets (at most kMaxRows); scrolls: more rows than that, so the scroll bar takes the right edge.
void ApplySidebar(uint8_t* m, const char* pkg, int rows, bool scrolls) {
  *reinterpret_cast<int*>(m + 0x134) = rows;  // visible widgets (ctor default 7): all of them; we scroll the rows
  // The group's real name ends with a space: hash 0x8D7E0C53 (FEngLib decode and the nfsu2-re
  // runtime object dump of UI_PauseOptionsMain.fng, docs.txt).
  uint8_t* nig = Obj(pkg, "NON_INTERACTIVE_GROUP ");
  uint8_t* big = Obj(pkg, "BIG MENU BACKING");
  Rect ref;
  if (!nig || !big || !ChildBounds(big, Pos(nig)[0] + Pos(big)[0], Pos(nig)[1] + Pos(big)[1], ref)) return;
  // Visible canvas: x = +-1/[0x79AC10] (WidescreenFix hor+ rewrites 2/640, Frontend.ixx), y = +-240.
  const float halfW = 1.0f / Var<float>(u2::addr::FECanvasScaleX);
  const Rect bar{-halfW + g_layout.margin, -240 + g_layout.margin, -halfW + g_layout.margin + g_layout.width,
                 240 - g_layout.margin};
  const float dxL = bar.l - ref.l, dxR = bar.r - ref.r, dyT = bar.t - ref.t;

  // Frame: black backing, white inner panel, green outline.
  for (const char* name : {"BIG MENU BACKING", "SMALLER MENU BACKING", "BORDER"}) {
    uint8_t* g = Obj(pkg, name);
    if (g) ReshapeFrame(g, Pos(nig)[0] + Pos(g)[0], Pos(nig)[1] + Pos(g)[1], ref, bar);
  }
  // Header texts follow the top-left / top-right corner.
  if (uint8_t* h = Obj(pkg, "HEADER_TEXT")) SetGeometry(h, Pos(h)[0] + dxL, Pos(h)[1] + dyT, Size(h)[0], Size(h)[1]);
  if (uint8_t* h = Obj(pkg, "HEADER_TEXT2")) SetGeometry(h, Pos(h)[0] + dxR, Pos(h)[1] + dyT, Size(h)[0], Size(h)[1]);

  // Rows (UIWidgetMenu fields, 0x51FF80): +0xF4/+0xF8 start (top-left of row 1), +0xFC/+0x100 next
  // row, +0x104 row width, +0x10C max title size, +0x114 max data size, +0x11C/+0x120 data top-left.
  // The arrows sit just outside the data column (toggle Position 0x52DAC0).
  auto f = [m](int off) -> float& { return *reinterpret_cast<float*>(m + off); };
  constexpr float kArrow = 24;  // arrow image width (JSON)
  // Gap to the bar edge: 16, plus a 32-unit column for the scroll bar when there is one (its arrows are 24 wide).
  constexpr float kScrollX = 26;  // scroll bar centre, from the bar's right edge
  constexpr float kEdgeGap = 16, kScrollColumn = 32;
  const float rightPad = kEdgeGap + (scrolls ? kScrollColumn : 0);
  // Rows end inside the inner panel (SMALLER MENU BACKING ends 32 above the outer frame) with a
  // small gap; shrink the step if they don't fit. The row step is +0x108 plus the spacing +0x128
  // (0x54F0BF), which the ctor sets to 0 (0x5528C2).
  const float rowsBottom = bar.b - 38;
  float startX = f(0xF4) + dxL, startY = f(0xF8) + dyT;
  if (rows > 0) f(0x108) = (std::min)(f(0x108), std::floor((rowsBottom - startY) / rows));
  float dataW = g_layout.valueWidth, dataX = bar.r - rightPad - kArrow - dataW;
  f(0xF4) = f(0xFC) = startX;
  f(0xF8) = f(0x100) = startY;
  f(0x11C) = dataX;
  f(0x120) = startY;
  f(0x114) = dataW;
  f(0x10C) = dataX - kArrow - startX;
  f(0x104) = std::fabs(startX - (dataX + dataW));

  // Parts this screen never uses: slider pieces, console back prompt, scroll bar.
  char name[32];
  for (const char* part : {"SLIDER_BEG_%d", "SLIDER_END_%d", "SLIDER_HANDLE_%d", "SLIDER_BASE_%d", "SLIDER_KNOB_%d"})
    for (int i = 1; i <= 7; ++i) {
      std::snprintf(name, sizeof name, part, i);
      HideObject(pkg, name);
    }
  if (scrolls) {
    PlaceScrollbar(m, pkg, bar.r - kScrollX, startY, startY + rows * f(0x108));
  } else {
    for (const char* n : {"U2_SCROLLER_CORNER", "U2_SCROLLER_CORNER(65)"}) HideObject(pkg, n);
    HideScrollbar(pkg);
  }
  // The tip sits in the strip between the inner panel (ends 32 above the outer frame) and the frame, left-aligned with
  // the row labels. Its top 28 above the frame centres the glyphs in the strip ([game] 2026-10-19 F12 at 1080p: with
  // 25 the text ran 999..1027 px in a 971..1041 px strip, 3 units low).
  if (g_cb.footer) PlaceFooter(pkg, startX, bar.b - 28);
  else HideObject(pkg, "UI_COMMON_BACK");
  U2LOG("menu: sidebar bar (%.0f,%.0f)-(%.0f,%.0f) from frame (%.0f,%.0f)-(%.0f,%.0f); rows x=%.0f y=%.0f step=%.0f data x=%.0f w=%.0f",
        bar.l, bar.t, bar.r, bar.b, ref.l, ref.t, ref.r, ref.b, startX, startY, f(0x108), dataX, dataW);
}

void* __cdecl Factory(void* showData) {
  if (!g_wantOurs) return g_origFactory(showData);
  g_wantOurs = false;
  void* menu = GameMalloc(0x15C);  // PauseOptionsFNGObject size
  if (!menu) return nullptr;
  UIWidgetMenu_ctor(menu, nullptr, showData);
  *static_cast<void**>(menu) = g_menuVtbl;
  auto m = static_cast<uint8_t*>(menu);
  const char* pkg = *reinterpret_cast<const char**>(m + 4);
  int n = g_cb.rowCount < kMaxRows ? g_cb.rowCount : kMaxRows;
  g_rowCount = g_cb.rowCount;
  g_top = 0;
  ApplySidebar(m, pkg, n, g_rowCount > n);

  u2::feng::Print(pkg, u2::feng::HashUpper("Header_text"), g_title.c_str());
  u2::feng::Print(pkg, u2::feng::HashUpper("Header_text2"), g_status.c_str());

  g_rows.clear();
  for (int i = 0; i < n; ++i) {
    void* w = GameMalloc(kWidgetSize);
    if (!w) break;
    FEToggleWidget_ctor(w, nullptr, reinterpret_cast<float*>(m + 0xFC), reinterpret_cast<float*>(m + 0x10C),
                        reinterpret_cast<float*>(m + 0x114), reinterpret_cast<float*>(m + 0x11C), true);
    *static_cast<void**>(w) = g_toggleVtbl;
    RowId(w) = i;
    g_rows.push_back(Row{static_cast<uint8_t*>(w), "", ""});
    UIWidgetMenu_AddToggleOption(menu, nullptr, w, true);  // binds OPTION_NAME_n etc. and calls Draw
  }
  UIWidgetMenu_SetInitialOption(menu, nullptr, 0);
  ApplyBacking(pkg);
  g_menu = menu;
  U2LOG("menu: built native screen %p with %zu row widgets for %d rows", menu, g_rows.size(), g_rowCount);
  return menu;
}
}  // namespace

bool IsMenuPackage(const char* name) { return g_open && name && _stricmp(name, kPkg) == 0; }

void SetRowCount(int rows) {
  if (!g_open) g_cb.rowCount = rows;
}

namespace {
// One bundle's UI_PauseOptions layout into g_layoutFile; false (logged) when the file is missing, damaged or has no
// such layout.
bool ReadLayoutFrom(const std::string& path) {
  HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (f == INVALID_HANDLE_VALUE) {
    U2LOG("menu: %s not readable (error %lu)", path.c_str(), GetLastError());
    return false;
  }
  std::vector<uint8_t> data(GetFileSize(f, nullptr));
  DWORD got = 0;
  const bool ok = !data.empty() && ReadFile(f, data.data(), static_cast<DWORD>(data.size()), &got, nullptr) &&
                  got == data.size();
  CloseHandle(f);
  if (ok && jdlz::IsJdlz(data.data(), data.size())) {
    std::vector<uint8_t> plain;
    if (!jdlz::Decompress(data.data(), data.size(), &plain)) {
      U2LOG("menu: %s: JDLZ data damaged", path.c_str());
      return false;
    }
    data.swap(plain);
  }
  size_t off = 0, len = 0;
  if (!ok || !fngchunk::Find(data.data(), data.size(), kPkg, &off, &len)) {
    U2LOG("menu: no %s layout in %s", kPkg, path.c_str());
    return false;
  }
  g_layoutFile.assign(data.begin() + off, data.begin() + off + len);
  U2LOG("menu: %s layout read from %s (offset 0x%zX, %zu bytes)", kPkg, path.c_str(), off, len);
  return true;
}

bool LoadLayoutFile() {
  if (!g_layoutFile.empty()) return true;
  char exe[MAX_PATH];
  DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return false;
  std::string dir(exe, n);
  dir = dir.substr(0, dir.find_last_of("\\/") + 1);
  for (const char* b : kLayoutBundles)
    if (ReadLayoutFrom(dir + b)) return true;
  U2LOG("menu: no %s layout found; no menu outside races", kPkg);
  return false;
}

// In a race the bundle has registered the layout; anywhere else we do, until Close.
bool EnsureLayout() {
  if (FindLayout(reinterpret_cast<void*>(kLayoutRegistry), nullptr, kPkg)) return true;
  if (!LoadLayoutFile()) return false;
  g_layoutChunk = g_layoutFile;
  g_layoutOurs = RegisterLayout(g_layoutChunk.data()) != 0;
  U2LOG("menu: %s layout %s", kPkg, g_layoutOurs ? "registered" : "REJECTED");
  return g_layoutOurs;
}

void ReleaseLayout() {
  if (!g_layoutOurs) return;
  UnregisterLayout(g_layoutChunk.data());
  g_layoutOurs = false;
  U2LOG("menu: %s layout unregistered", kPkg);
}

bool IsActivePackage(u2::feng::FEPackage* p) {
  for (auto q = u2::feng::FirstActivePackage(); q; q = u2::feng::NextPackage(q))
    if (q == p) return true;
  return false;
}

// Every other active package's input off, with the game's own setter. A package the game gives input again while we are
// open keeps that as the value to give back (it is what the game wants once we close).
void MaskOthers() {
  for (auto p = u2::feng::FirstActivePackage(); p; p = u2::feng::NextPackage(p)) {
    const char* name = u2::feng::PackageName(p);
    if (!name || _stricmp(name, kPkg) == 0) continue;
    uint32_t* m = u2::feng::PackageMasks(p);
    Masked* e = nullptr;
    for (auto& k : g_masked)
      if (k.pkg == p) e = &k;
    if (!(m[0] | m[1] | m[2] | m[3])) {  // off already: by us, or the game's own choice, given back as it is
      if (!e) g_masked.push_back({p, name, {0, 0, 0, 0}});
      continue;
    }
    if (!e) {
      g_masked.push_back({p, name, {}});
      e = &g_masked.back();
    }
    std::memcpy(e->masks, m, sizeof e->masks);
    u2::feng::SetControlMask(name, 0);
    if (g_verbose) U2LOG("menu: input off for %s (mask %X)", name, e->masks[0]);
  }
}

// Back as they were, for the packages still up. The setter writes one value into all four fields; a package whose
// fields differed gets the other three back as they were.
void UnmaskOthers() {
  for (const auto& e : g_masked) {
    if (!IsActivePackage(e.pkg) || _stricmp(u2::feng::PackageName(e.pkg), e.name.c_str()) != 0) continue;
    u2::feng::SetControlMask(e.name.c_str(), e.masks[0]);
    uint32_t* m = u2::feng::PackageMasks(e.pkg);
    for (int i = 1; i < 4; ++i) m[i] = e.masks[i];
  }
  if (g_verbose && !g_masked.empty()) U2LOG("menu: input back for %zu packages", g_masked.size());
  g_masked.clear();
}
}  // namespace

bool Install(const Callbacks& cb, const Layout& layout, bool verbose, FactoryMode mode) {
  g_cb = cb;
  g_layout = layout;
  g_verbose = verbose;
  g_mode = mode;
  auto cur = Var<FactoryFn>(kFactorySlot);
  if (mode == FactoryMode::Permanent && reinterpret_cast<uintptr_t>(cur) != 0x4EB650) {
    U2LOG("menu: FNGData[%s] factory is %p, expected 004EB650 (another mod?); menu unavailable", kPkg, cur);
    return false;
  }
  std::memcpy(g_menuVtbl, reinterpret_cast<void*>(0x795CDC), sizeof g_menuVtbl);     // UIWidgetMenu vtable
  g_menuVtbl[2] = reinterpret_cast<void*>(&MenuNotify);
  std::memcpy(g_toggleVtbl, reinterpret_cast<void*>(0x79B3B8), sizeof g_toggleVtbl);  // FEToggleWidget vtable
  g_toggleVtbl[1] = reinterpret_cast<void*>(&ToggleAct);
  g_toggleVtbl[3] = reinterpret_cast<void*>(&ToggleDraw);
  g_toggleVtbl[5] = reinterpret_cast<void*>(&ToggleShow);
  if (mode == FactoryMode::WhileOpening) {
    U2LOG("menu: ready (factory borrowed only while opening)");
    return true;
  }
  g_origFactory = cur;
  u2::mem::Write<FactoryFn>(kFactorySlot, &Factory);
  U2LOG("menu: factory installed");
  return true;
}

void Uninstall() {
  if (g_mode == FactoryMode::Permanent && g_origFactory) u2::mem::Write<FactoryFn>(kFactorySlot, g_origFactory);
  g_origFactory = nullptr;
}

bool Open() {
  if (g_open) return true;
  if (IsFNGLoaded(kPkg)) {
    U2LOG("menu: %s already loaded, not opening", kPkg);
    return false;
  }
  if (!EnsureLayout()) return false;
  g_menu = nullptr;
  g_pendingActivate = -1;
  g_pendingBack = false;
  g_joyHandlersWereOn = Var<int>(kJoyHandlersOn) != 0;
  g_wantOurs = true;
  // The screen's FNGObject::ctor syncs the PC help bar to its FNGData help mask (0x551F43..0x551F67 -> SyncByMask
  // 0x54E6E0; UI_PauseOptions' is 0x10400, Back only), and removing a package syncs nothing back, so the screen under
  // ours kept a Back-only bar after we closed ([game] 2026-11-03, main menu, crib, car lot). With the mask borrowed as
  // 0 for this synchronous push the ctor skips the sync (0x551F46) and the bar stays as its screen set it.
  const uint32_t helpMask = Var<uint32_t>(kHelpMaskSlot);
  u2::mem::Write<uint32_t>(kHelpMaskSlot, 0);
  if (g_mode == FactoryMode::WhileOpening) {
    // Borrow the slot for this one synchronous push (the factory runs inside it, [game] 2026-10-02)
    // and chain to whatever was there for any other construction.
    g_origFactory = Var<FactoryFn>(kFactorySlot);
    u2::mem::Write<FactoryFn>(kFactorySlot, &Factory);
    AddFngToUIObject_1(kPkg, nullptr);
    u2::mem::Write<FactoryFn>(kFactorySlot, g_origFactory);
  } else {
    AddFngToUIObject_1(kPkg, nullptr);  // runs our factory synchronously
  }
  u2::mem::Write<uint32_t>(kHelpMaskSlot, helpMask);
  g_wantOurs = false;
  if (!g_menu) {
    U2LOG("menu: factory was not called for %s", kPkg);
    RemoveFngAndJoyHandlers(kPkg);
    if (g_joyHandlersWereOn) AddJoyHandlers(Var<void*>(u2::addr::pFEng), nullptr);
    ReleaseLayout();
    return false;
  }
  g_savedKeyboardMap = Var<uint8_t>(kMenuKeyboardMap);
  SetMenuKeyboardMap(1);
  // While a restore from the last close is pending, the block byte still holds our 1.
  SharedPause& sp = Pause();
  if (sp.openMenus == 0 && sp.releaseIn == 0) sp.saved = Var<uint8_t>(kPauseKeyBlock);
  sp.releaseIn = 0;
  InterlockedIncrement(&sp.openMenus);
  Var<uint8_t>(kPauseKeyBlock) = 1;
  g_inputLogIn = g_verbose ? 10 : 0;
  g_open = true;
  if (g_cb.modal) MaskOthers();
  U2LOG("menu: opened");
  return true;
}

// The FE skips a package's pad/keyboard input entirely when its port mask pkg+0x24 is 0
// (ProcessPadsForPackage 0x54D4B0, check at 0x54D4BC; docs/kb/fe-input.md).
// Hidden or input-blocked: the mask is 0; both off: the saved one is back.
void SetPortMask(bool hidden, bool blocked) {
  const bool wasOff = g_hidden || g_inputBlocked, off = hidden || blocked;
  if (wasOff != off)
    if (auto p = reinterpret_cast<uint8_t*>(u2::feng::FindPackage(kPkg))) {
      auto& mask = *reinterpret_cast<uint32_t*>(p + 0x24);
      if (off) {
        g_savedPortMask = mask;
        mask = 0;
      } else {
        mask = g_savedPortMask;
      }
    }
  g_hidden = hidden;
  g_inputBlocked = blocked;
}

void SetHidden(bool hidden) {
  if (g_open && hidden != g_hidden) SetPortMask(hidden, g_inputBlocked);
}

void SetInputBlocked(bool blocked) {
  if (g_open && blocked != g_inputBlocked) SetPortMask(g_hidden, blocked);
}

void SetFooter(const char* footer) {
  g_cb.footer = footer;
  if (!g_open) return;
  if (uint8_t* text = Obj(kPkg, "BACK_BUTTON")) SetFooterText(text);
}

bool IsHidden() { return g_open && g_hidden; }

void SetGameInput(bool on) {
  if (!g_open || on == g_gameInput) return;
  g_gameInput = on;
  SetMenuKeyboardMap(on ? static_cast<char>(g_savedKeyboardMap) : 1);
  U2LOG("menu: %s keyboard map", on ? "game" : "menu");
}

void Close() {
  if (!g_open) return;
  SetPortMask(false, false);
  g_gameInput = false;
  FEngSetLastButton(kPkg, 0);
  RemoveFngAndJoyHandlers(kPkg);
  // The joy handlers are global, not per package: if another screen needs them (registered
  // before us, or a pause screen that came up meanwhile), register them again.
  if (g_joyHandlersWereOn || u2::IsGamePaused()) AddJoyHandlers(Var<void*>(u2::addr::pFEng), nullptr);
  UnmaskOthers();  // the screens under ours get their input back once ours is gone, as when a game screen closes
  ReleaseLayout();
  SetMenuKeyboardMap(g_savedKeyboardMap);
  // Restore the pause-key block a few frames later so an Esc still held from closing can't pause.
  SharedPause& sp = Pause();
  if (InterlockedDecrement(&sp.openMenus) <= 0) {
    sp.openMenus = 0;
    sp.releaseIn = 3;
  }
  g_open = false;
  g_menu = nullptr;
  g_rows.clear();
  U2LOG("menu: closed");
}

bool IsOpen() { return g_open; }

// Restores the saved pause-key block, unless the game changed it meanwhile (it writes the byte
// itself at 0x501221, 0x50161D and 0x5004B0).
void Tick() {
  SharedPause& sp = Pause();
  if (sp.releaseIn > 0 && InterlockedDecrement(&sp.releaseIn) == 0 && sp.openMenus == 0 &&
      Var<uint8_t>(kPauseKeyBlock) == 1)
    Var<uint8_t>(kPauseKeyBlock) = sp.saved;
}

void Update(const std::string& title, const std::string& status) {
  if (!g_open) return;
  if (g_cb.modal) MaskOthers();  // a screen that came up meanwhile
  if (g_inputLogIn > 0 && --g_inputLogIn == 0) {
    auto pkg = reinterpret_cast<uint8_t*>(u2::feng::FindPackage(kPkg));
    U2LOG("menu input: mapctx=%d menuMap=%d joyHandlers=%d pkg+24=%X", Var<int>(kKeyboardMapCtx),
          Var<uint8_t>(kMenuKeyboardMap), Var<int>(kJoyHandlersOn), pkg ? *reinterpret_cast<uint32_t*>(pkg + 0x24) : 0);
  }
  if (title != g_title) {
    g_title = title;
    u2::feng::Print(kPkg, u2::feng::HashUpper("Header_text"), g_title.c_str());
  }
  if (status != g_status) {
    g_status = status;
    u2::feng::Print(kPkg, u2::feng::HashUpper("Header_text2"), g_status.c_str());
  }
  for (auto& r : g_rows) DrawRow(r.widget, false);
  if (g_cb.footer) ShowFooter(kPkg);  // in case the package's opening pass hides ConsoleOnly objects after the factory
  if (g_cb.isEnabled)
    for (auto& r : g_rows) SyncEnabled(r.widget, g_cb.isEnabled(RowId(r.widget)));
  if (g_pendingActivate >= 0) {
    int row = g_pendingActivate;
    g_pendingActivate = -1;
    g_cb.activate(row);
  }
  if (g_pendingBack) {
    g_pendingBack = false;
    g_cb.back();
  }
}
}  // namespace u2::optmenu
