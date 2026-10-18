#include "u2sdk/ini.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>

#include "u2sdk/log.h"

namespace u2 {

Ini::Ini() : path_(log::ModuleDir() + log::ModuleName() + ".ini") {}

std::string Ini::GetString(const char* section, const char* key, const char* def) const {
  char buf[512] = {};
  GetPrivateProfileStringA(section, key, def, buf, sizeof(buf), path_.c_str());
  std::string s = buf;
  // strip trailing inline comment and whitespace
  if (auto c = s.find(';'); c != std::string::npos) s.erase(c);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
  return s;
}

int Ini::GetInt(const char* section, const char* key, int def) const {
  std::string s = GetString(section, key, "");
  if (s.empty()) return def;
  // Decimal unless written 0x...: base 0 read "08" / "09" as octal and gave 0 (2026-10-18 review).
  bool hex = s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
  return static_cast<int>(std::strtol(s.c_str(), nullptr, hex ? 16 : 10));
}

bool Ini::GetBool(const char* section, const char* key, bool def) const {
  std::string s = GetString(section, key, "");
  if (s.empty()) return def;
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (s == "true" || s == "yes" || s == "on") return true;
  if (s == "false" || s == "no" || s == "off") return false;
  return GetInt(section, key, def) != 0;
}

float Ini::GetFloat(const char* section, const char* key, float def) const {
  std::string s = GetString(section, key, "");
  return s.empty() ? def : static_cast<float>(std::atof(s.c_str()));
}

int ParseVirtualKey(const std::string& raw) {
  std::string n = raw;
  std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  if (n.empty()) return 0;
  if (n.rfind("0X", 0) == 0) return static_cast<int>(std::strtol(n.c_str(), nullptr, 16));
  if (n.size() == 1 && ((n[0] >= 'A' && n[0] <= 'Z') || (n[0] >= '0' && n[0] <= '9'))) return n[0];
  if (n[0] == 'F' && n.size() <= 3) {
    int f = std::atoi(n.c_str() + 1);
    if (f >= 1 && f <= 24) return VK_F1 + f - 1;
  }
  struct { const char* name; int vk; } table[] = {
      {"TAB", VK_TAB},         {"SPACE", VK_SPACE},     {"ENTER", VK_RETURN},  {"RETURN", VK_RETURN},
      {"ESC", VK_ESCAPE},      {"ESCAPE", VK_ESCAPE},   {"BACKSPACE", VK_BACK}, {"INSERT", VK_INSERT},
      {"DELETE", VK_DELETE},   {"HOME", VK_HOME},       {"END", VK_END},       {"PAGEUP", VK_PRIOR},
      {"PAGEDOWN", VK_NEXT},   {"UP", VK_UP},           {"DOWN", VK_DOWN},     {"LEFT", VK_LEFT},
      {"RIGHT", VK_RIGHT},     {"SHIFT", VK_SHIFT},     {"CTRL", VK_CONTROL},  {"CONTROL", VK_CONTROL},
      {"ALT", VK_MENU},        {"CAPSLOCK", VK_CAPITAL}, {"PAUSE", VK_PAUSE},  {"SCROLLLOCK", VK_SCROLL},
      {"NUMPAD0", VK_NUMPAD0}, {"OEM3", VK_OEM_3},      {"TILDE", VK_OEM_3},
      {"MINUS", VK_OEM_MINUS}, {"-", VK_OEM_MINUS},  {"EQUALS", VK_OEM_PLUS}, {"=", VK_OEM_PLUS},
      {"LBRACKET", VK_OEM_4},  {"[", VK_OEM_4},       {"RBRACKET", VK_OEM_6}, {"]", VK_OEM_6},
      {"BACKSLASH", VK_OEM_5},
  };
  for (auto& e : table)
    if (n == e.name) return e.vk;
  return 0;
}
}  // namespace u2
