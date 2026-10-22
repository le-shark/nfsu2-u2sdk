#pragma once
// Direct3D 9 helpers: hooking device methods (by vtable index, via MinHook on the d3d9.dll
// implementation) and getting told when the game is about to Reset the device.
struct IDirect3DDevice9;
struct _D3DSURFACE_DESC;

namespace u2::d3d {

// The backbuffer's description (size, format, MultiSampleType: in-game FSAA). False when it can't be read.
bool BackBufferDesc(IDirect3DDevice9* dev, _D3DSURFACE_DESC* out);

namespace vt {
constexpr int Present = 17;
constexpr int EndScene = 42;
}

// Hooks method `index` of the game's device. Returns false if the device doesn't exist yet.
bool HookDeviceMethod(int index, void* detour, void** original, const char* name);

// Called right before the game resets the device (game handler 0x5D1C40, e.g. after alt-tab).
// WidescreenFix inline-hooks the same entry (Resolution.ixx); both chain, but never disable or
// remove this hook at runtime, or WSF's jump would be overwritten.
using ResetListener = void (*)();
void AddResetListener(ResetListener fn);
// Called right after the game's handler returns, with its result (the Reset HRESULT; 0 = the device is back).
using PostResetListener = void (*)(int hr);
void AddPostResetListener(PostResetListener fn);
bool InstallResetHook();   // call once at init (hooks 0x5D1C40)

}  // namespace u2::d3d
