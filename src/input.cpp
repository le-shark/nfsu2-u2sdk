#include "u2sdk/input.h"

#include <windows.h>
#include <xinput.h>

#include <cmath>
#include <cstring>

#include "u2sdk/game.h"

namespace u2 {
namespace {
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
  wheel_ = 0;
  if (focus) {
    wheel_ = Var<int>(addr::MouseWheel) / WHEEL_DELTA;
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

  // Pad
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

bool Input::KeyRepeat(int vk, float dt, float delay, float rate) {
  if (!KeyDown(vk)) { held_[vk] = 0; return false; }
  if (KeyPressed(vk)) { held_[vk] = 0; nextRep_[vk] = delay; return true; }
  held_[vk] += dt;
  if (held_[vk] >= nextRep_[vk]) { nextRep_[vk] += rate; return true; }
  return false;
}

bool Input::PadRepeat(uint16_t b, float dt, float delay, float rate) {
  int i = 0;
  while (i < 16 && !((1u << i) & b)) ++i;
  if (i == 16) return false;
  if (!PadDown(b)) { padHeld_[i] = 0; return false; }
  if (PadPressed(b)) { padHeld_[i] = 0; padNext_[i] = delay; return true; }
  padHeld_[i] += dt;
  if (padHeld_[i] >= padNext_[i]) { padNext_[i] += rate; return true; }
  return false;
}

void Input::SetMouseCapture(bool on) {
  capture_ = on;
  warm_ = false;
}
}  // namespace u2
