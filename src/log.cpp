#include "u2sdk/log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace u2::log {
namespace {
FILE* g_file = nullptr;
CRITICAL_SECTION g_cs;
bool g_init = false;

std::string ModulePath() {
  char buf[MAX_PATH] = {};
  GetModuleFileNameA(reinterpret_cast<HMODULE>(&__ImageBase), buf, MAX_PATH);
  return buf;
}
}  // namespace

std::string ModuleDir() {
  std::string p = ModulePath();
  return p.substr(0, p.find_last_of("\\/") + 1);
}

std::string ModuleName() {
  std::string p = ModulePath();
  std::string n = p.substr(p.find_last_of("\\/") + 1);
  return n.substr(0, n.find_last_of('.'));
}

void Init() {
  if (g_init) return;
  InitializeCriticalSection(&g_cs);
  g_init = true;
  // The previous session's log is kept as <name>.prev.log (2026-10-24: a crash's CRASH lines were lost when the game
  // was started again and the log rewritten).
  std::string base = ModuleDir() + ModuleName();
  MoveFileExA((base + ".log").c_str(), (base + ".prev.log").c_str(), MOVEFILE_REPLACE_EXISTING);
  g_file = std::fopen((base + ".log").c_str(), "w");
}

void Write(const char* fmt, ...) {
  if (!g_init || !g_file) return;
  EnterCriticalSection(&g_cs);
  SYSTEMTIME t;
  GetLocalTime(&t);
  std::fprintf(g_file, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
  va_list va;
  va_start(va, fmt);
  std::vfprintf(g_file, fmt, va);
  va_end(va);
  std::fputc('\n', g_file);
  std::fflush(g_file);
  LeaveCriticalSection(&g_cs);
}
}  // namespace u2::log
