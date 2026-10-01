#pragma once
// Polled input for scripts: keyboard (GetAsyncKeyState, only while the game window has focus),
// relative mouse (cursor re-centering while captured), mouse wheel (game's DInput lZ) and the
// first connected XInput pad. Call Update() once per game frame.
#include <cstdint>

namespace u2 {

enum PadButton : uint16_t {
  PadUp = 0x0001, PadDown = 0x0002, PadLeft = 0x0004, PadRight = 0x0008,
  PadStart = 0x0010, PadBack = 0x0020, PadLS = 0x0040, PadRS = 0x0080,
  PadLB = 0x0100, PadRB = 0x0200, PadA = 0x1000, PadB = 0x2000, PadX = 0x4000, PadY = 0x8000,
};

struct PadState {
  bool connected = false;
  uint16_t buttons = 0;
  float lx = 0, ly = 0, rx = 0, ry = 0;  // -1..1 after deadzone
  float lt = 0, rt = 0;                  // 0..1
};

class Input {
 public:
  void Update();

  bool KeyDown(int vk) const { return vk > 0 && vk < 256 && cur_[vk]; }
  bool KeyPressed(int vk) const { return vk > 0 && vk < 256 && cur_[vk] && !prev_[vk]; }
  // Auto-repeating press: true on press, then every `rate` s after `delay` s held.
  bool KeyRepeat(int vk, float dt, float delay = 0.35f, float rate = 0.06f);

  const PadState& Pad() const { return pad_; }
  bool PadDown(uint16_t b) const { return (pad_.buttons & b) != 0; }
  bool PadPressed(uint16_t b) const { return (pad_.buttons & b) && !(prevButtons_ & b); }
  bool PadRepeat(uint16_t b, float dt, float delay = 0.35f, float rate = 0.08f);

  // Relative mouse: when captured, the cursor is re-centred in the game window every frame.
  void SetMouseCapture(bool on);
  float MouseDX() const { return mdx_; }
  float MouseDY() const { return mdy_; }
  int Wheel() const { return wheel_; }  // notches this frame (+ = away from user)

  float stickDeadzone = 0.15f;

 private:
  bool cur_[256] = {}, prev_[256] = {};
  float held_[256] = {}, nextRep_[256] = {};
  PadState pad_;
  uint16_t prevButtons_ = 0;
  float padHeld_[16] = {}, padNext_[16] = {};
  bool capture_ = false, warm_ = false;
  float mdx_ = 0, mdy_ = 0;
  int wheel_ = 0;
  unsigned long lastPadProbe_ = 0;
};

}  // namespace u2
