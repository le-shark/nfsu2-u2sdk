#include "u2sdk/mem.h"

#include <windows.h>

#include <cstring>

namespace u2::mem {

void WriteBytes(uintptr_t addr, const void* data, size_t size) {
  DWORD old;
  VirtualProtect(reinterpret_cast<void*>(addr), size, PAGE_EXECUTE_READWRITE, &old);
  std::memcpy(reinterpret_cast<void*>(addr), data, size);
  VirtualProtect(reinterpret_cast<void*>(addr), size, old, &old);
  FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(addr), size);
}

void MakeJmp(uintptr_t at, const void* to) {
  unsigned char buf[5] = {0xE9};
  int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(to) - (at + 5));
  std::memcpy(buf + 1, &rel, 4);
  WriteBytes(at, buf, 5);
}
}  // namespace u2::mem
