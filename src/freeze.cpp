#include "u2sdk/freeze.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "u2sdk/log.h"
#include "u2sdk/mem.h"

namespace u2::freeze {
namespace {
constexpr uintptr_t kPauseDecision = 0x58132C, kPauseDecisionReturn = 0x581331;
constexpr uint8_t kPauseDecisionBytes[5] = {0x0F, 0xB6, 0xC0, 0x3B, 0xC3};  // movzx eax,al; cmp eax,ebx
constexpr uint32_t kMagic = 0x5A455246;  // "FREZ"

// One per process, mapped by every u2sdk ASI. Holders are bits; slots are handed out in load order.
struct Shared {
  uint32_t magic;
  volatile LONG holders;   // offset 4: read by the stub
  volatile LONG nextSlot;
  uint32_t installed;      // the pause decision points to some ASI's stub
};

Shared* g_shared = nullptr;  // read by the stub of the ASI that patched the site
LONG g_bit = 0;

__declspec(naked) void PauseDecisionStub() {
  __asm {
    movzx eax, al
    push ecx
    mov ecx, g_shared
    cmp dword ptr [ecx + 4], 0
    pop ecx
    je done
    mov eax, 1
  done:
    cmp eax, ebx
    push kPauseDecisionReturn
    ret
  }
}

bool Map() {
  if (g_shared) return true;
  char name[64];
  std::snprintf(name, sizeof name, "Local\\NFSU2_u2sdk_freeze_%lu", GetCurrentProcessId());
  HANDLE h = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(Shared), name);
  if (!h) return false;
  auto* s = static_cast<Shared*>(MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
  if (!s) return false;  // the handle stays open for the process lifetime, like the view
  if (s->magic != kMagic) {  // first ASI: the block is zero-filled
    s->magic = kMagic;
  }
  LONG slot = InterlockedIncrement(&s->nextSlot) - 1;
  if (slot >= 31) return false;
  g_bit = 1L << slot;
  g_shared = s;
  return true;
}
}  // namespace

bool Install() {
  if (!Map()) {
    U2LOG("freeze: shared block unavailable");
    return false;
  }
  if (g_shared->installed) {
    U2LOG("freeze: pause decision already redirected by another ASI; sharing it");
    return true;
  }
  if (std::memcmp(reinterpret_cast<void*>(kPauseDecision), kPauseDecisionBytes, 5) != 0) {
    U2LOG("freeze: pause decision @ %08X has unexpected bytes (another mod?)", static_cast<unsigned>(kPauseDecision));
    return false;
  }
  mem::MakeJmp(kPauseDecision, reinterpret_cast<void*>(&PauseDecisionStub));
  g_shared->installed = 1;
  U2LOG("freeze: pause decision @ %08X redirected", static_cast<unsigned>(kPauseDecision));
  return true;
}

bool Attach() { return Map(); }

void Hold(bool on) {
  if (!g_shared) return;
  if (on) InterlockedOr(&g_shared->holders, g_bit);
  else InterlockedAnd(&g_shared->holders, ~g_bit);
}

bool Held() { return g_shared && (g_shared->holders & g_bit) != 0; }
bool AnyHeld() { return g_shared && g_shared->holders != 0; }

}  // namespace u2::freeze
