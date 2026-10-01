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

void Nop(uintptr_t addr, size_t size) {
  unsigned char buf[64];
  while (size) {
    size_t n = size < sizeof(buf) ? size : sizeof(buf);
    std::memset(buf, 0x90, n);
    WriteBytes(addr, buf, n);
    addr += n;
    size -= n;
  }
}

static void MakeRel(uintptr_t at, const void* to, unsigned char op) {
  unsigned char buf[5];
  buf[0] = op;
  int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(to) - (at + 5));
  std::memcpy(buf + 1, &rel, 4);
  WriteBytes(at, buf, 5);
}

void MakeJmp(uintptr_t at, const void* to) { MakeRel(at, to, 0xE9); }
void MakeCall(uintptr_t at, const void* to) { MakeRel(at, to, 0xE8); }

uintptr_t BranchTarget(uintptr_t at) {
  auto op = *reinterpret_cast<unsigned char*>(at);
  if (op != 0xE8 && op != 0xE9) return 0;
  return at + 5 + *reinterpret_cast<int32_t*>(at + 1);
}
}  // namespace u2::mem
