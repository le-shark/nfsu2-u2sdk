#pragma once
// Plain-text log file next to the ASI: scripts\<ModuleName>.log (truncated on each game start).
#include <string>

namespace u2::log {
void Init();                              // call once from DllMain / init
void Write(const char* fmt, ...);         // printf-style, adds timestamp + newline, flushes
std::string ModuleDir();                  // directory of this ASI, with trailing backslash
std::string ModuleName();                 // ASI file name without extension
}  // namespace u2::log

#define U2LOG(...) ::u2::log::Write(__VA_ARGS__)
