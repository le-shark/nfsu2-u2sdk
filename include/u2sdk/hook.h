#pragma once
// Function hooks via MinHook (function entry points only; MinHook cooperates with other
// MinHook/safetyhook users and with hooks already present as a leading JMP).
//
// Hooking a __thiscall game function: declare the detour as __fastcall(this, edx, args...)
// and the original pointer type the same way; call the original as orig(self, edx, args...).
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace u2::hook {

// Entry-byte check before hooking: the expected bytes (from u2re), or a leading JMP when another of our ASIs (or
// another MinHook user) hooked the same function first; MinHook chains onto that (lessons.md section 1).
inline bool EntryIs(uintptr_t at, const char* expect, size_t n) {
  const auto* p = reinterpret_cast<const unsigned char*>(at);
  return *p == 0xE9 || std::memcmp(p, expect, n) == 0;
}

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
