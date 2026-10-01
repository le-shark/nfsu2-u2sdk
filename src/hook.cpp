#include "u2sdk/hook.h"

#include <MinHook.h>

#include "u2sdk/log.h"

namespace u2::hook {

bool Init() {
  MH_STATUS s = MH_Initialize();
  if (s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED) {
    U2LOG("MinHook init failed: %s", MH_StatusToString(s));
    return false;
  }
  return true;
}

bool Create(uintptr_t target, void* detour, void** original, const char* name) {
  MH_STATUS s = MH_CreateHook(reinterpret_cast<void*>(target), detour, original);
  if (s != MH_OK) {
    U2LOG("hook %s @ %08X failed: %s", name, static_cast<unsigned>(target), MH_StatusToString(s));
    return false;
  }
  U2LOG("hook %s @ %08X created", name, static_cast<unsigned>(target));
  return true;
}

bool EnableAll() {
  MH_STATUS s = MH_EnableHook(MH_ALL_HOOKS);
  if (s != MH_OK) U2LOG("MH_EnableHook failed: %s", MH_StatusToString(s));
  return s == MH_OK;
}

void DisableAll() { MH_DisableHook(MH_ALL_HOOKS); }
}  // namespace u2::hook
