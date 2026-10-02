#include "u2sdk/game.h"

#include <windows.h>

namespace u2 {

bool IsSupportedExe() {
  auto base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
  auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
  auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  return base + nt->OptionalHeader.AddressOfEntryPoint == addr::kEntryPoint;
}

int GameFlowState() { return Var<int>(addr::GameFlowState); }
bool IsInGame() { return GameFlowState() == 6; }

bool IsGamePaused() {
  if (Var<uint8_t>(addr::PauseFEActive)) return true;
  auto rc = Var<int*>(addr::pRaceCoordinator);  // GameLoop 0x581319: cmp [eax], 6
  return rc && *rc == 6;
}

HWND Window() { return Var<HWND>(addr::hWnd); }

bool WindowHasFocus() {
  HWND w = Window();
  return w && GetForegroundWindow() == w && !Var<uint8_t>(addr::IsLostFocus);
}

IDirect3DDevice9* Device() { return Var<IDirect3DDevice9*>(addr::D3DDevice); }
CameraParams* PlayerCamera() { return reinterpret_cast<CameraParams*>(addr::PlayerCamera); }
eView* GetView(int id) { return reinterpret_cast<eView*>(addr::Views + id * 0x70); }

bool PlayerCarPosition(Vec3* out) {
  auto car = Var<uint8_t*>(addr::PlayerCarState);
  if (!car) return false;
  auto p = reinterpret_cast<float*>(car + 0x50);
  *out = {p[0], p[1], p[2]};
  return true;
}

int RacerCars(RacerCar* out, int max) {
  auto w = Var<uint8_t*>(addr::pWorld);
  if (!w) return 0;
  int total = *reinterpret_cast<int*>(w + 0x10), players = *reinterpret_cast<int*>(w + 0x14);
  int n = *reinterpret_cast<int*>(w + 0x18);
  if (n > total) n = total;
  if (n > kMaxCars) n = kMaxCars;
  auto cars = reinterpret_cast<uint8_t**>(w + 0x1C);
  int k = 0;
  for (int i = 0; i < n && k < max; ++i) {
    uint8_t* c = cars[i];
    if (!c) continue;
    bool player = i < players;
    if (!player && *reinterpret_cast<int*>(c + 0x28) == 0) continue;
    out[k++] = {c, player};
  }
  return k;
}

void CreateLookAtMatrix(bMatrix4* out, const Vec3& from, const Vec3& to, const Vec3& up) {
  alignas(16) bVector4 f{from.x, from.y, from.z, 1}, t{to.x, to.y, to.z, 1}, u{up.x, up.y, up.z, 0};
  using Fn = void(__cdecl*)(bMatrix4*, bVector4*, bVector4*, bVector4*);
  reinterpret_cast<Fn>(addr::eCreateLookAtMatrix)(out, &f, &t, &u);
}
}  // namespace u2
