#pragma once
// Logs access violations, illegal instructions, divide-by-zero and stack overflows (first chance,
// at most a few) with the faulting address (module+offset), registers and top of stack, so in-game
// crashes can be diagnosed from the log.
namespace u2::crash {
void InstallLogger();
// "module.dll+offset" for a code address (or the bare address).
void Describe(unsigned long addr, char* out, unsigned n);
}
