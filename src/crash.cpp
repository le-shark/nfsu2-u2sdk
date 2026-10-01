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
  char where[300];
  Where(c->Eip, where, sizeof where);
  U2LOG("CRASH %08X at %s (EIP %08X) access %s %08X", code, where, c->Eip,
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
  return EXCEPTION_CONTINUE_SEARCH;
}
}  // namespace

void InstallLogger() { AddVectoredExceptionHandler(1, Handler); }
}  // namespace u2::crash
