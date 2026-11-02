#include "u2sdk/game.h"

#include <windows.h>

#include "u2sdk/log.h"

namespace u2 {

bool IsSupportedExe() {
  auto base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
  auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
  auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  return base + nt->OptionalHeader.AddressOfEntryPoint == addr::kEntryPoint;
}

int GameFlowState() { return Var<int>(addr::GameFlowState); }
bool IsInGame() { return GameFlowState() == 6; }

bool IsPauseMenuState() {
  auto rc = Var<int*>(addr::pRaceCoordinator);  // GameLoop 0x581319: cmp [eax], 6
  return rc && *rc == 6;
}

bool IsGamePaused() { return Var<uint8_t>(addr::PCHelpBarUp) || IsPauseMenuState(); }

bool IsNetworkSession() {
  using Fn = bool(__fastcall*)(void*, void*);  // thiscall(Network*); true if [ecx+0x21], [0x8669F4] or [0x866A14]
  return reinterpret_cast<Fn>(0x5F00F0)(reinterpret_cast<void*>(addr::Network), nullptr);
}

HWND Window() { return Var<HWND>(addr::hWnd); }

bool WindowHasFocus() {
  HWND w = Window();
  return w && GetForegroundWindow() == w && !Var<uint8_t>(addr::IsLostFocus);
}

IDirect3DDevice9* Device() { return Var<IDirect3DDevice9*>(addr::D3DDevice); }
CameraParams* PlayerCamera() { return reinterpret_cast<CameraParams*>(addr::PlayerCamera); }
uint8_t* PlayerCar() { return Var<uint8_t*>(addr::CarSlots); }

namespace {
// World car array (see game.h).
struct WorldCars {
  int total, players, racers;  // racers = players + AI racers
  uint8_t** cars;
};
bool GetWorldCars(WorldCars* out) {
  auto w = Var<uint8_t*>(addr::pWorld);
  if (!w) return false;
  int total = *reinterpret_cast<int*>(w + 0x10);
  *out = {total < kMaxCars ? total : kMaxCars, *reinterpret_cast<int*>(w + 0x14), *reinterpret_cast<int*>(w + 0x18),
          reinterpret_cast<uint8_t**>(w + 0x1C)};
  return true;
}
int Field(const uint8_t* p, int off) { return *reinterpret_cast<const int*>(p + off); }
}  // namespace

int RacerCars(RacerCar* out, int max) {
  WorldCars w;
  if (!GetWorldCars(&w)) return 0;
  int n = w.racers < w.total ? w.racers : w.total, k = 0;
  for (int i = 0; i < n && k < max; ++i) {
    uint8_t* c = w.cars[i];
    if (!c) continue;
    bool player = i < w.players;
    if (!player && Field(c, 0x28) == 0) continue;
    out[k++] = {c, player ? 0 : i - w.players + 1};
  }
  return k;
}

int ActiveCars(uint8_t** out, int max) {
  WorldCars w;
  if (!GetWorldCars(&w)) return 0;
  int k = 0;
  for (int i = 0; i < w.total && k < max; ++i) {
    uint8_t* c = w.cars[i];
    if (c && (i < w.players || Field(c, 0x28) != 0)) out[k++] = c;
  }
  return k;
}

int FrontEndCars(uint8_t** out, int max) {
  int k = 0;
  const auto head = reinterpret_cast<uint8_t*>(kFeCarList);
  for (uint8_t* c = *reinterpret_cast<uint8_t**>(head); c && c != head && k < max; c = *reinterpret_cast<uint8_t**>(c)) {
    if (!*reinterpret_cast<uint8_t**>(c + 0x08) || !Field(c, 0xB64) || Field(c, 0x900) != 1) continue;
    Vec3 axes[3];
    FeCarAxes(c, axes);
    bool unit = true;
    for (const Vec3& a : axes) unit &= Length(a) > 0.9f && Length(a) < 1.1f;
    if (unit) out[k++] = c;
  }
  return k;
}

// The game's own stepped segment test, copied from 0x446970 (a path-blocking check):
//   0x4469DE  ctor 0x5E0B40 on a 0x314-byte stack query object (thiscall, no args)
//   per step: 0x446B15  0x5E6FA0(obj, &mid, |b-a|*0.5 + 0.5, 0)   fill polygon cache, ret 0xC
//             0x446B31  0x5D39F0(obj, 1, 1, &a, &b, 0.5f, 1.0f)   nearest hit, ret 0x18
//   0x446B5F / 0x446B8A  dtor 0x5E0BF0 on both exits
// 0x5D39F0 mode 1 on a hit moves b to a + normalize(b - a) * (hitDist - margin); it does not fill
// the cache itself. Steps are 30 m, the game's own chunk for long segments (0x5E7ED0, constant
// 0x7A4644), which also keeps a fill under the cache's 160-polygon cap (cmp 0xA0 at 0x5E1044).
// CubicCameraMover::Update uses the same test for camera wall push-back (0x4536C7).
bool WorldRayHit(const Vec3& from, const Vec3& dir, float maxLen, float* dist) {
  if (!Var<uint8_t*>(addr::pWorld)) return false;
  using CtorFn = void*(__fastcall*)(void*, void*);
  using FillFn = void(__fastcall*)(void*, void*, const bVector4*, float, int);
  using SegFn = bool(__fastcall*)(void*, void*, int, int, const bVector4*, bVector4*, float, float);
  constexpr float kStep = 30, kMargin = 0.5f;
  alignas(16) uint8_t obj[0x320];
  reinterpret_cast<CtorFn>(0x5E0B40)(obj, nullptr);
  bool hit = false;
  for (float s = 0; s < maxLen && !hit; s += kStep) {
    float e = s + kStep < maxLen ? s + kStep : maxLen;
    Vec3 p0 = from + dir * s, p1 = from + dir * e, m = (p0 + p1) * 0.5f;
    alignas(16) bVector4 a{p0.x, p0.y, p0.z, 1}, b{p1.x, p1.y, p1.z, 1}, mid{m.x, m.y, m.z, 1};
    reinterpret_cast<FillFn>(0x5E6FA0)(obj, nullptr, &mid, (e - s) * 0.5f + 0.5f, 0);
    if (reinterpret_cast<SegFn>(0x5D39F0)(obj, nullptr, 1, 1, &a, &b, kMargin, 1.0f)) {
      *dist = s + Dot(Vec3{b.x, b.y, b.z} - p0, dir) + kMargin;
      hit = true;
    }
  }
  using DtorFn = void(__fastcall*)(void*, void*);
  reinterpret_cast<DtorFn>(0x5E0BF0)(obj, nullptr);
  return hit;
}

void LogCars() {
  WorldCars w;
  if (!GetWorldCars(&w)) return;
  U2LOG("cars: total=%d players=%d racers+players=%d", w.total, w.players, w.racers);
  for (int i = 0; i < w.total; ++i) {
    uint8_t* c = w.cars[i];
    if (!c) continue;
    auto di = *reinterpret_cast<uint8_t**>(c + 0x14);
    Vec3 p = CarPosition(c), a[3];
    CarAxes(c, a);
    U2LOG("  car %d %p driverType=%d class=%d mode=%d pos (%.1f %.1f %.1f) fwd (%.2f %.2f %.2f) up (%.2f %.2f %.2f)", i, c,
          di ? Field(di, 4) : -1, Field(c, 0x24), Field(c, 0x28), p.x, p.y, p.z, a[0].x, a[0].y, a[0].z, a[2].x, a[2].y, a[2].z);
  }
}

void CreateLookAtMatrix(bMatrix4* out, const Vec3& from, const Vec3& to, const Vec3& up) {
  alignas(16) bVector4 f{from.x, from.y, from.z, 1}, t{to.x, to.y, to.z, 1}, u{up.x, up.y, up.z, 0};
  using Fn = void(__cdecl*)(bMatrix4*, bVector4*, bVector4*, bVector4*);
  reinterpret_cast<Fn>(addr::eCreateLookAtMatrix)(out, &f, &t, &u);
}
}  // namespace u2
