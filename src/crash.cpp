#include "u2sdk/crash.h"

#include <windows.h>

#include "u2sdk/log.h"

namespace u2::crash {
namespace {
LONG g_logged = 0;

void Where(uintptr_t a, char* out, size_t n) {
  HMODULE m = nullptr;
  char name[MAX_PATH] = "?";
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(a), &m) && m) {
    GetModuleFileNameA(m, name, MAX_PATH);
    const char* base = strrchr(name, '\\');
    _snprintf_s(out, n, _TRUNCATE, "%s+%X", base ? base + 1 : name, static_cast<unsigned>(a - reinterpret_cast<uintptr_t>(m)));
  } else {
    _snprintf_s(out, n, _TRUNCATE, "%08X", static_cast<unsigned>(a));
  }
}

LONG CALLBACK Handler(EXCEPTION_POINTERS* ep) {
  DWORD code = ep->ExceptionRecord->ExceptionCode;
  if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_INT_DIVIDE_BY_ZERO &&
      code != EXCEPTION_STACK_OVERFLOW)
    return EXCEPTION_CONTINUE_SEARCH;
  if (InterlockedIncrement(&g_logged) > 4) return EXCEPTION_CONTINUE_SEARCH;
  CONTEXT* c = ep->ContextRecord;
  // A vectored handler sees every exception first, also ones the game, a driver or another mod catches and handles:
  // the line says so, and a crash is the last of these lines before the log ends (2026-10-18 review).
  if (code == EXCEPTION_STACK_OVERFLOW) {  // too little stack left for the trail below
    U2LOG("CRASH? stack overflow at EIP %08X (first-chance exception)", c->Eip);
    return EXCEPTION_CONTINUE_SEARCH;
  }
  char where[300];
  Where(c->Eip, where, sizeof where);
  U2LOG("CRASH? %08X at %s (EIP %08X) access %s %08X (first-chance exception: a crash if the log ends here)", code,
        where, c->Eip,
        ep->ExceptionRecord->NumberParameters > 1 ? (ep->ExceptionRecord->ExceptionInformation[0] ? "write" : "read") : "?",
        ep->ExceptionRecord->NumberParameters > 1 ? static_cast<unsigned>(ep->ExceptionRecord->ExceptionInformation[1]) : 0u);
  U2LOG("  EAX %08X EBX %08X ECX %08X EDX %08X ESI %08X EDI %08X EBP %08X ESP %08X", c->Eax, c->Ebx, c->Ecx, c->Edx,
        c->Esi, c->Edi, c->Ebp, c->Esp);
  auto sp = reinterpret_cast<const uintptr_t*>(c->Esp);
  for (int i = 0; i < 24; ++i) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(sp + i, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT) break;
    char w[300];
    Where(sp[i], w, sizeof w);
    U2LOG("  [esp+%02X] %08X %s", i * 4, static_cast<unsigned>(sp[i]), w);
  }
  // The faulting address: freed or never mapped (a stale pointer) versus mapped but protected.
  if (ep->ExceptionRecord->NumberParameters > 1) {
    MEMORY_BASIC_INFORMATION mbi;
    auto a = reinterpret_cast<const void*>(ep->ExceptionRecord->ExceptionInformation[1]);
    if (VirtualQuery(a, &mbi, sizeof mbi))
      U2LOG("  fault address: state %s, protect %08X, region %08X + %X", mbi.State == MEM_COMMIT ? "commit"
            : mbi.State == MEM_RESERVE ? "reserve" : "free", static_cast<unsigned>(mbi.Protect),
            static_cast<unsigned>(reinterpret_cast<uintptr_t>(mbi.BaseAddress)), static_cast<unsigned>(mbi.RegionSize));
  }
  // Deeper call trail: stack words that point into executable module code (likely return addresses), to 0x1000 bytes.
  for (int i = 24; i < 0x400; ++i) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(sp + i, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT) break;
    if (!VirtualQuery(reinterpret_cast<const void*>(sp[i]), &mbi, sizeof mbi) || mbi.Type != MEM_IMAGE ||
        !(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
      continue;
    char w[300];
    Where(sp[i], w, sizeof w);
    U2LOG("  trail [esp+%03X] %s", i * 4, w);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}
}  // namespace

void InstallLogger() { AddVectoredExceptionHandler(1, Handler); }
void Describe(unsigned long addr, char* out, unsigned n) { Where(addr, out, n); }
}  // namespace u2::crash
