#pragma once
// Function hooks via MinHook (function entry points only; MinHook cooperates with other
// MinHook/safetyhook users and with hooks already present as a leading JMP).
//
// Hooking a __thiscall game function: declare the detour as __fastcall(this, edx, args...)
// and the original pointer type the same way; call the original as orig(self, edx, args...).
#include <cstdint>

namespace u2::hook {

bool Init();
bool Create(uintptr_t target, void* detour, void** original, const char* name);
bool Enable(uintptr_t target);
bool EnableAll();
void DisableAll();

template <typename F>
inline bool Create(uintptr_t target, F detour, F* original, const char* name) {
  return Create(target, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(original), name);
}

}  // namespace u2::hook
