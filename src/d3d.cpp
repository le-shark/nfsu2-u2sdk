#include "u2sdk/d3d.h"

#include <d3d9.h>

#include <vector>

#include "u2sdk/game.h"
#include "u2sdk/hook.h"
#include "u2sdk/log.h"

namespace u2::d3d {
namespace {
std::vector<ResetListener> g_listeners;
using ResetHandlerFn = void(__cdecl*)();
ResetHandlerFn g_origReset = nullptr;

void __cdecl ResetHandlerDetour() {
  for (auto fn : g_listeners) fn();
  g_origReset();
}
}  // namespace

bool HookDeviceMethod(int index, void* detour, void** original, const char* name) {
  IDirect3DDevice9* dev = Device();
  if (!dev) return false;
  void** vtbl = *reinterpret_cast<void***>(dev);
  auto target = reinterpret_cast<uintptr_t>(vtbl[index]);
  return hook::Create(target, detour, original, name) && hook::Enable(target);
}

void AddResetListener(ResetListener fn) { g_listeners.push_back(fn); }

bool InstallResetHook() {
  // The game's "device needs reset" handler, called from the frame function when
  // TestCooperativeLevel returns D3DERR_DEVICENOTRESET. It takes no stack arguments.
  return hook::Create(addr::GameResetDevice, &ResetHandlerDetour, &g_origReset, "GameResetDevice");
}
}  // namespace u2::d3d
