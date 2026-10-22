#pragma once
// Logs access violations, illegal instructions, divide-by-zero and stack overflows (first chance,
// at most a few) with the faulting address (module+offset), registers and top of stack, so in-game
// crashes can be diagnosed from the log.
#include <cstdint>

namespace u2::crash {
void InstallLogger();

// Every loaded module outside the Windows folder (the game, the ASIs, proxy DLLs such as a d3d9.dll in the game
// folder), one line each with its load address: a tester's mod list without asking for it.
void LogModules();
}  // namespace u2::crash
