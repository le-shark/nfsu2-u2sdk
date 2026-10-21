#include "u2sdk/crash.h"

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include "u2sdk/log.h"

namespace u2::crash {
namespace {
LONG g_logged = 0;
}  // namespace
void LogModules();
namespace {

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

// Deeper call trail: stack words that point into executable module code (likely return addresses), to 0x1000 bytes.
void Trail(const uintptr_t* sp, int from) {
  for (int i = from; i < 0x400; ++i) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(sp + i, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT) break;
    if (!VirtualQuery(reinterpret_cast<const void*>(sp[i]), &mbi, sizeof mbi) || mbi.Type != MEM_IMAGE ||
        !(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
      continue;
    char w[300];
    Where(sp[i], w, sizeof w);
    U2LOG("  trail [esp+%03X] %s", i * 4, w);
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
  Trail(sp, 24);
  if (g_logged == 1) LogModules();  // the mods loaded at the first crash line (user 2026-10-21)
  return EXCEPTION_CONTINUE_SEARCH;
}

// ---- write watch --------------------------------------------------------------------------------
uintptr_t g_watchAddr = 0;
const char* g_watchName = "";
LONG g_watchLogged = 0;

// The CPU raises a single-step exception after the watched write; DR6 bit 0 says breakpoint 0 fired.
LONG CALLBACK WatchHandler(EXCEPTION_POINTERS* ep) {
  if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || !g_watchAddr || !(ep->ContextRecord->Dr6 & 1))
    return EXCEPTION_CONTINUE_SEARCH;
  CONTEXT* c = ep->ContextRecord;
  c->Dr6 &= ~0xFu;
  if (InterlockedIncrement(&g_watchLogged) <= 6) {
    char where[300];
    Where(c->Eip, where, sizeof where);
    U2LOG("WRITE %s [%08X] = %u by the instruction before %s (EIP %08X), thread %lu", g_watchName,
          static_cast<unsigned>(g_watchAddr), *reinterpret_cast<const volatile uint32_t*>(g_watchAddr), where, c->Eip,
          GetCurrentThreadId());
    Trail(reinterpret_cast<const uintptr_t*>(c->Esp), 0);
  }
  return EXCEPTION_CONTINUE_EXECUTION;
}

struct ArmJob {
  DWORD self;  // the helper thread: no breakpoint there
  int armed = 0, busy = 0;
};

// Debug registers of another thread can only be set while it is suspended, so a helper thread does it for every
// thread of the process, the caller included (the caller waits on the helper meanwhile).
DWORD WINAPI ArmThreads(LPVOID p) {
  auto* job = static_cast<ArmJob*>(p);
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE) return 0;
  THREADENTRY32 te{sizeof te};
  for (BOOL more = Thread32First(snap, &te); more; more = Thread32Next(snap, &te)) {
    if (te.th32OwnerProcessID != GetCurrentProcessId() || te.th32ThreadID == job->self) continue;
    HANDLE t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, te.th32ThreadID);
    if (!t) continue;
    if (SuspendThread(t) != static_cast<DWORD>(-1)) {
      CONTEXT c{};
      c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
      if (GetThreadContext(t, &c)) {
        if (c.Dr7 & 3) {
          ++job->busy;  // breakpoint 0 already used by someone else: leave it
        } else {
          c.Dr0 = g_watchAddr;
          c.Dr7 = (c.Dr7 & ~0xF0000u) | 1u | (1u << 16) | (3u << 18);  // local enable, break on write, 4 bytes
          if (SetThreadContext(t, &c)) ++job->armed;
        }
      }
      ResumeThread(t);
    }
    CloseHandle(t);
  }
  CloseHandle(snap);
  return 0;
}
}  // namespace

void InstallLogger() { AddVectoredExceptionHandler(1, Handler); }

// No heap and no snapshot (the heap may be the thing that broke): the module list from the PEB into a static array.
void LogModules() {
  static HMODULE mods[512];
  static char winDir[MAX_PATH], path[MAX_PATH];
  const UINT winLen = GetWindowsDirectoryA(winDir, MAX_PATH);
  DWORD bytes = 0;
  if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof mods, &bytes)) {
    U2LOG("modules: list failed (%lu)", GetLastError());
    return;
  }
  const int count = static_cast<int>((bytes < sizeof mods ? bytes : sizeof mods) / sizeof(HMODULE));
  int n = 0;
  for (int i = 0; i < count; ++i) {
    if (!GetModuleFileNameA(mods[i], path, MAX_PATH)) continue;
    if (winLen && _strnicmp(path, winDir, winLen) == 0) continue;  // Windows' own and the drivers
    WIN32_FILE_ATTRIBUTE_DATA fa{};
    char date[16] = "?";
    SYSTEMTIME st;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &fa) && FileTimeToSystemTime(&fa.ftLastWriteTime, &st))
      _snprintf_s(date, sizeof date, _TRUNCATE, "%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
    U2LOG("module %s (%lu KB, %s)", path, fa.nFileSizeLow / 1024, date);
    ++n;
  }
  U2LOG("modules: %d outside %s", n, winDir);
}

bool WatchWrites(uintptr_t addr, const char* name) {
  if (g_watchAddr) return false;
  g_watchAddr = addr;
  g_watchName = name;
  AddVectoredExceptionHandler(1, WatchHandler);
  ArmJob job{};
  HANDLE h = CreateThread(nullptr, 0, ArmThreads, &job, CREATE_SUSPENDED, &job.self);
  if (!h) {
    U2LOG("watch %s: no helper thread (%lu)", name, GetLastError());
    return false;
  }
  ResumeThread(h);
  WaitForSingleObject(h, INFINITE);  // job lives on this stack
  CloseHandle(h);
  U2LOG("watch %s [%08X]: write breakpoint in %d threads (%d had breakpoint 0 in use)", name,
        static_cast<unsigned>(addr), job.armed, job.busy);
  return job.armed > 0;
}
}  // namespace u2::crash
