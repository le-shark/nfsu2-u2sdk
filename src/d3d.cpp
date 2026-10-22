#include "u2sdk/d3d.h"

#include <d3d9.h>

#include <vector>

#include "u2sdk/game.h"
#include "u2sdk/hook.h"
#include "u2sdk/log.h"

namespace u2::d3d {
namespace {
std::vector<ResetListener> g_listeners;
std::vector<PostResetListener> g_postListeners;
// int __cdecl(void): returns the last IDirect3DDevice9::Reset HRESULT in eax (0x5D1D91 / 0x5D1E11;
// docs/research/2026-10-09-daylight-evidence/route-b-effects.md section 4).
using ResetHandlerFn = int(__cdecl*)();
ResetHandlerFn g_origReset = nullptr;

int __cdecl ResetHandlerDetour() {
  for (auto fn : g_listeners) fn();
  int hr = g_origReset();
  for (auto fn : g_postListeners) fn(hr);
  return hr;
}
}  // namespace

bool BackBufferDesc(IDirect3DDevice9* dev, D3DSURFACE_DESC* out) {
  IDirect3DSurface9* bb = nullptr;
  if (!dev || FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) return false;
  const bool ok = SUCCEEDED(bb->GetDesc(out));
  bb->Release();
  return ok;
}

bool HookDeviceMethod(int index, void* detour, void** original, const char* name) {
  IDirect3DDevice9* dev = Device();
  if (!dev) return false;
  void** vtbl = *reinterpret_cast<void***>(dev);
  auto target = reinterpret_cast<uintptr_t>(vtbl[index]);
  return hook::Create(target, detour, original, name) && hook::Enable(target);
}

void AddResetListener(ResetListener fn) { g_listeners.push_back(fn); }
void AddPostResetListener(PostResetListener fn) { g_postListeners.push_back(fn); }

bool InstallResetHook() {
  // The game's "device needs reset" handler, called from the frame function when
  // TestCooperativeLevel returns D3DERR_DEVICENOTRESET. It takes no stack arguments.
  return hook::Create(addr::GameResetDevice, &ResetHandlerDetour, &g_origReset, "GameResetDevice");
}
}  // namespace u2::d3d
