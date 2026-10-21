#pragma once
// Logs access violations, illegal instructions, divide-by-zero and stack overflows (first chance,
// at most a few) with the faulting address (module+offset), registers and top of stack, so in-game
// crashes can be diagnosed from the log.
#include <cstdint>

namespace u2::crash {
void InstallLogger();

// Every loaded module outside the Windows folder (the game, the ASIs, proxy DLLs such as a d3d9.dll in the game
// folder), one line each: a tester's mod list without asking for it.
void LogModules();

// A hardware write breakpoint (debug register 0, 4 bytes) on addr in every thread running now: each write logs
// the writing instruction (module+offset) and a call trail, at most a few times. Threads started later are not
// covered. Diagnostics only (2026-10-21, a tester's shader-reload crash). Returns at once; a helper thread arms the
// threads one by one and logs how many.
bool WatchWrites(uintptr_t addr, const char* name);
}  // namespace u2::crash
