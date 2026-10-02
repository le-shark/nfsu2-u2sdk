#pragma once
// Direct3D 9 helpers: hooking device methods (by vtable index, via MinHook on the d3d9.dll
// implementation) and getting told when the game is about to Reset the device.
struct IDirect3DDevice9;

namespace u2::d3d {

namespace vt {
constexpr int EndScene = 42;
}

// Hooks method `index` of the game's device. Returns false if the device doesn't exist yet.
bool HookDeviceMethod(int index, void* detour, void** original, const char* name);

// Called right before the game resets the device (game handler 0x5D1C40, e.g. after alt-tab).
// WidescreenFix inline-hooks the same entry (Resolution.ixx); both chain, but never disable or
// remove this hook at runtime, or WSF's jump would be overwritten.
using ResetListener = void (*)();
void AddResetListener(ResetListener fn);
bool InstallResetHook();   // call once at init (hooks 0x5D1C40)

}  // namespace u2::d3d
