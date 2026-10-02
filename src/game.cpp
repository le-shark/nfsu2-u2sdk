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
