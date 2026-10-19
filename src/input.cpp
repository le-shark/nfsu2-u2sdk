#include "u2sdk/input.h"

#include <windows.h>
#include <xinput.h>

#include <cmath>
#include <cstring>

#include "u2sdk/game.h"
#include "u2sdk/log.h"

namespace u2 {
namespace {
// Mouse wheel. The game reads its DInput mouse in 0x5BF860 (ecx = 0x8763D0, lZ stored at 0x8763EC), called from
// 0x5D2C97. NFSU-XtendedInput redirects that call to its own reader (dllmain.cpp:2069) and never writes the wheel, so
// with it (or without a DInput mouse) the wheel comes from the window's WM_MOUSEWHEEL messages instead
// (docs/kb/fe-input.md). Only one source is used at a time, so a notch is never counted twice.
constexpr uintptr_t kMouseReadCall = 0x5D2C97, kMouseRead = 0x5BF860, kMouseDevice = 0x8763D0;
volatile LONG g_msgWheel = 0;  // WM_MOUSEWHEEL deltas not yet taken
HHOOK g_msgHook = nullptr;
bool g_msgHookTried = false;

LRESULT CALLBACK WheelHook(int code, WPARAM wp, LPARAM lp) {
  // PM_REMOVE only: a message peeked without removal would be seen again.
  if (code == HC_ACTION && wp == PM_REMOVE) {
    auto m = reinterpret_cast<const MSG*>(lp);
    if (m->message == WM_MOUSEWHEEL) InterlockedExchangeAdd(&g_msgWheel, GET_WHEEL_DELTA_WPARAM(m->wParam));
  }
  return CallNextHookEx(g_msgHook, code, wp, lp);
}

// A thread message hook on the game window's thread: it leaves the window procedure chain (WidescreenFix,
// XtendedInput) alone.
void EnsureWheelHook() {
  if (g_msgHookTried) return;
  HWND w = Window();
  if (!w) return;
  g_msgHookTried = true;
  g_msgHook = SetWindowsHookExA(WH_GETMESSAGE, WheelHook, nullptr, GetWindowThreadProcessId(w, nullptr));
  if (!g_msgHook) U2LOG("input: window message hook failed (%lu); the wheel works only through the game", GetLastError());
}

bool GameReadsMouse() {
  auto p = reinterpret_cast<const uint8_t*>(kMouseReadCall);
  int32_t rel;
  std::memcpy(&rel, p + 1, 4);
  return p[0] == 0xE8 && kMouseReadCall + 5 + rel == kMouseRead && Var<void*>(kMouseDevice) != nullptr;
}

// The game handles one window message per frame: its pump 0x5BF1C0 (called once per GameLoop pass, 0x58155D)
// peeks, then translates and dispatches a single message. With a backlog a wheel message waited several frames
// ([game] 2026-10-19: "sometimes it lags behind"). So the wheel messages are taken out of the queue at once and
// dispatched as the pump would (every window procedure still sees them); the hook counts them on the way.
void PumpWheelMessages() {
  MSG m;
  for (int i = 0; i < 64 && PeekMessageA(&m, nullptr, WM_MOUSEWHEEL, WM_MOUSEWHEEL, PM_REMOVE); ++i) {
    TranslateMessage(&m);
    DispatchMessageA(&m);
  }
}

int TakeWheel(bool focus) {
  EnsureWheelHook();
  if (g_msgHook && !GameReadsMouse()) PumpWheelMessages();
  LONG msg = InterlockedExchange(&g_msgWheel, 0);
  static LONG rest = 0;  // high-resolution wheels send parts of a notch
  if (!focus) {
    rest = 0;
    return 0;
  }
  static int source = -1;  // logged once per change: 1 game, 0 messages
  bool game = GameReadsMouse();
  if (int(game) != source) {
    source = int(game);
    U2LOG("input: mouse wheel from %s", game ? "the game's mouse read" : "window messages (the game's mouse read is replaced)");
  }
  if (game) {
    rest = 0;
    return Var<int>(addr::MouseWheel) / WHEEL_DELTA;
  }
  rest += msg;
  int notches = rest / WHEEL_DELTA;
  rest -= notches * WHEEL_DELTA;
  return notches;
}

float Stick(SHORT v, float dz) {
  float f = v / 32767.0f;
  if (f < -1) f = -1;
  float a = std::fabs(f);
  if (a < dz) return 0;
  return (f > 0 ? 1 : -1) * (a - dz) / (1 - dz);
}

bool WindowPoint(POINT* p, float fx, float fy) {
  HWND w = Window();
  RECT rc;
  if (!w || !GetClientRect(w, &rc)) return false;
  p->x = static_cast<LONG>((rc.right - rc.left) * fx);
  p->y = static_cast<LONG>((rc.bottom - rc.top) * fy);
  return ClientToScreen(w, p) != FALSE;
}
}  // namespace

void Input::Update() {
  std::memcpy(prev_, cur_, sizeof(cur_));
  prevButtons_ = pad_.buttons;
  bool focus = WindowHasFocus();

  for (int vk = 1; vk < 256; ++vk) cur_[vk] = focus && (GetAsyncKeyState(vk) & 0x8000) != 0;

  // Mouse
  mdx_ = mdy_ = 0;
  wheel_ = TakeWheel(focus);
  if (focus) {
    POINT c, p;
    if (capture_ && WindowPoint(&c, capX_, capY_) && GetCursorPos(&p)) {
      if (warm_) {
        mdx_ = static_cast<float>(p.x - c.x);
        mdy_ = static_cast<float>(p.y - c.y);
      }
      SetCursorPos(c.x, c.y);
      warm_ = true;
    }
  } else {
    warm_ = false;
  }

  UpdatePad(focus);
}

void Input::UpdateOnly(const int* vks, int n, bool pad) {
  std::memcpy(prev_, cur_, sizeof(cur_));
  prevButtons_ = pad_.buttons;
  bool focus = WindowHasFocus();
  std::memset(cur_, 0, sizeof(cur_));
  for (int i = 0; i < n; ++i)
    if (vks[i] > 0 && vks[i] < 256) cur_[vks[i]] = focus && (GetAsyncKeyState(vks[i]) & 0x8000) != 0;
  mdx_ = mdy_ = 0;
  wheel_ = 0;
  // Scrolls while idle must not arrive at the next full Update (queued ones included).
  if (g_msgHook && !GameReadsMouse()) PumpWheelMessages();
  InterlockedExchange(&g_msgWheel, 0);
  warm_ = false;
  if (pad) UpdatePad(focus);
  else pad_ = PadState{};
}

void Input::UpdatePad(bool focus) {
  // XInputGetState on an empty slot is slow, so only retry about once a second.
  XINPUT_STATE st{};
  bool wasConnected = pad_.connected;
  pad_ = PadState{};
  DWORD now = GetTickCount();
  bool poll = focus && (wasConnected || now - lastPadProbe_ > 1000);
  if (poll && !wasConnected) lastPadProbe_ = now;
  if (poll && XInputGetState(0, &st) == ERROR_SUCCESS) {
    auto& g = st.Gamepad;
    pad_.connected = true;
    pad_.buttons = g.wButtons;
    pad_.lx = Stick(g.sThumbLX, stickDeadzone);
    pad_.ly = Stick(g.sThumbLY, stickDeadzone);
    pad_.rx = Stick(g.sThumbRX, stickDeadzone);
    pad_.ry = Stick(g.sThumbRY, stickDeadzone);
    pad_.lt = g.bLeftTrigger > 30 ? (g.bLeftTrigger - 30) / 225.0f : 0;
    pad_.rt = g.bRightTrigger > 30 ? (g.bRightTrigger - 30) / 225.0f : 0;
  }
}

void Input::SetMouseCapture(bool on) {
  capture_ = on;
  warm_ = false;
}
}  // namespace u2
